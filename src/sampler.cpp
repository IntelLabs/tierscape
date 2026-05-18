#include "sampler.h"
#include "util.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

Sampler::Sampler(const Config& cfg, pid_t target_pid)
    : m_cfg(cfg), m_target_pid(target_pid) {}

Sampler::~Sampler() {
    stop();
    if (m_pipe_fd >= 0) {
        ::close(m_pipe_fd);
        m_pipe_fd = -1;
    }
}

bool Sampler::spawn() {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        log_err("pipe() failed: %s", std::strerror(errno));
        return false;
    }

    // Build shell command for the pipeline.
    std::string events_str;
    for (size_t i = 0; i < m_cfg.events.size(); ++i) {
        if (i) events_str += " ";
        events_str += "-e " + m_cfg.events[i];
    }

    char cmd[4096];
    std::snprintf(cmd, sizeof(cmd),
        "exec %s record -d %s -c %d -p %d -o - 2>/dev/null | "
        "exec %s script -i - --fields=time,addr 2>/dev/null",
        m_cfg.perf_bin.c_str(), events_str.c_str(),
        m_cfg.frequency, m_target_pid,
        m_cfg.perf_bin.c_str());

    log_verbose("Sampler pipeline: %s", cmd);

    pid_t pid = fork();
    if (pid < 0) {
        log_err("fork() failed: %s", std::strerror(errno));
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        return false;
    }

    if (pid == 0) {
        // Child: new session so we can killpg the entire pipeline.
        setsid();

        // Redirect stdout -> write end of pipe.
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) _exit(127);
        ::close(pipefd[0]);
        ::close(pipefd[1]);

        // exec /bin/sh -c <cmd>
        execl("/bin/sh", "sh", "-c", cmd, nullptr);
        _exit(127);
    }

    // Parent.
    ::close(pipefd[1]);
    m_pipe_fd    = pipefd[0];
    m_child_pgid = pid;  // we called setsid() in the child so child PID == its PGID
    return true;
}

void Sampler::shutdown_child() {
    pid_t pgid = m_child_pgid;
    if (pgid > 0) {
        // SIGTERM the whole process group, then reap.
        killpg(pgid, SIGTERM);
        // Give it a moment, then SIGKILL if still alive.
        for (int i = 0; i < 20; ++i) {
            int status;
            pid_t r = waitpid(pgid, &status, WNOHANG);
            if (r == pgid || r < 0) {
                m_child_pgid = -1;
                return;
            }
            struct timespec ts = {0, 50 * 1000 * 1000};  // 50 ms
            nanosleep(&ts, nullptr);
        }
        killpg(pgid, SIGKILL);
        waitpid(pgid, nullptr, 0);
        m_child_pgid = -1;
    }
}

void Sampler::run(SampleCallback cb) {
    if (!spawn()) return;

    m_running = true;

    // Buffered line reader over the raw fd.
    constexpr size_t BUF_SZ = 8192;
    char buf[BUF_SZ];
    std::string carry;
    carry.reserve(256);

    while (!m_stop_requested.load(std::memory_order_relaxed)) {
        ssize_t n = read(m_pipe_fd, buf, BUF_SZ);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;  // EOF

        carry.append(buf, buf + n);

        size_t start = 0;
        while (true) {
            size_t nl = carry.find('\n', start);
            if (nl == std::string::npos) break;

            // Process line [start, nl)
            const char* line = carry.data() + start;
            const char* end  = carry.data() + nl;
            start = nl + 1;

            // Find ':' separator: "<time>: <addr>"
            const char* colon = static_cast<const char*>(memchr(line, ':', end - line));
            if (!colon) continue;
            const char* p = colon + 1;
            while (p < end && (*p == ' ' || *p == '\t')) ++p;
            if (p >= end) continue;

            char* eptr = nullptr;
            uint64_t addr = std::strtoull(p, &eptr, 16);

            if (addr < 0x1000ULL) continue;
            // x86_64 userspace upper bound is 0x7fff_ffff_ffff; everything
            // beyond is kernel.
            if (addr >= 0x800000000000ULL) continue;

            const uint64_t pgsz = static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
            addr &= ~(pgsz - 1);

            m_samples_seen.fetch_add(1, std::memory_order_relaxed);
            cb(addr);
        }
        carry.erase(0, start);
    }

    m_running = false;
    shutdown_child();
    if (m_pipe_fd >= 0) {
        ::close(m_pipe_fd);
        m_pipe_fd = -1;
    }
    log_verbose("Sampler stopped (%lu samples seen)",
                m_samples_seen.load());
}

void Sampler::stop() {
    m_stop_requested.store(true, std::memory_order_relaxed);
    // Tell the child to die. The reader thread will see EOF and exit.
    if (m_child_pgid > 0) {
        killpg(m_child_pgid, SIGTERM);
    }
}
