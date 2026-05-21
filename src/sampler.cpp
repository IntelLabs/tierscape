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

namespace {
// Architecture-portable userspace upper bound. On x86_64 (4-level paging)
// userspace ends at 0x0000_7fff_ffff_ffff. On 5-level paging or other
// architectures the bound differs; we err on the permissive side and let
// the migrator drop addresses outside any VMA later.
constexpr uint64_t userspace_upper_bound() {
#if defined(__x86_64__)
    return 0x800000000000ULL;
#elif defined(__aarch64__)
    // 48-bit VA is the common AArch64 user limit.
    return 0x1000000000000ULL;
#else
    // Fallback: accept anything below the canonical 63-bit boundary.
    return 0x8000000000000000ULL;
#endif
}

constexpr size_t kCarryMaxBytes = 1 << 20;  // 1 MiB safety cap.
}  // namespace

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

    // Build shell command for the pipeline. Events are validated in
    // config_load (is_valid_event_name) so this is safe to interpolate.
    std::string events_str;
    for (size_t i = 0; i < m_cfg.events.size(); ++i) {
        if (i) events_str += " ";
        events_str += "-e " + m_cfg.events[i];
    }

    std::string cmd;
    cmd.reserve(events_str.size() + m_cfg.perf_bin.size() * 2 + 256);
    cmd  = "exec ";   cmd += m_cfg.perf_bin;
    cmd += " record -d ";    cmd += events_str;
    cmd += " -c " + std::to_string(m_cfg.frequency);
    cmd += " -p " + std::to_string(m_target_pid);
    cmd += " -o - 2>/dev/null | exec ";
    cmd += m_cfg.perf_bin;
    cmd += " script -i - --fields=time,addr 2>/dev/null";

    log_verbose("Sampler pipeline: %s", cmd.c_str());

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
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
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

    const uint64_t pgsz       = static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
    const uint64_t pg_mask    = ~(pgsz - 1);
    const uint64_t user_upper = userspace_upper_bound();
    bool carry_overflow_warned = false;

    auto parse_one_line = [&](const char* line, const char* end) {
        // perf script with --fields=time,addr emits:
        //   "<time>: <addr>"
        // Time may have a decimal point ("1234.567890"); we accept either.
        char* eptr = nullptr;
        double t_sec = std::strtod(line, &eptr);
        uint64_t time_ns = 0;
        const char* p = (eptr && eptr > line) ? eptr : line;
        if (eptr && eptr > line && t_sec >= 0.0) {
            time_ns = static_cast<uint64_t>(t_sec * 1e9);
        }
        const char* colon = static_cast<const char*>(memchr(p, ':', end - p));
        if (!colon) return;
        p = colon + 1;
        while (p < end && (*p == ' ' || *p == '\t')) ++p;
        if (p >= end) return;

        eptr = nullptr;
        uint64_t addr = std::strtoull(p, &eptr, 16);
        if (eptr == p) return;

        if (addr < 0x1000ULL || addr >= user_upper) return;
        addr &= pg_mask;

        m_samples_seen.fetch_add(1, std::memory_order_relaxed);
        cb(time_ns, addr);
    };

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
            parse_one_line(carry.data() + start, carry.data() + nl);
            start = nl + 1;
        }
        carry.erase(0, start);

        // Bound the carry so a malformed stream without newlines
        // cannot grow it without limit.
        if (carry.size() > kCarryMaxBytes) {
            if (!carry_overflow_warned) {
                log_warn("Sampler: discarding %zu bytes of malformed input "
                         "(no newline within %zu bytes)",
                         carry.size(), kCarryMaxBytes);
                carry_overflow_warned = true;
            }
            carry.clear();
        }
    }

    // Flush any final partial line on EOF (perf may not terminate the
    // last record with '\n').
    if (!carry.empty()) {
        parse_one_line(carry.data(), carry.data() + carry.size());
        carry.clear();
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
