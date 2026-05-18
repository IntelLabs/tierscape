#include "sampler.h"
#include "util.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

Sampler::Sampler(const Config& cfg, pid_t pid)
    : m_cfg(cfg), m_pid(pid) {}

Sampler::~Sampler() {
    stop();
}

void Sampler::run(SampleCallback cb) {
    // Build perf command:
    // perf record -d -e <events> -c <period> -p <pid> -o - |
    //   perf script -i - --fields=time,addr
    //
    // Uses -d for data address recording, -c for period-based sampling,
    // and streams via -o - | perf script -i -

    std::string events_str;
    for (size_t i = 0; i < m_cfg.events.size(); i++) {
        if (i > 0) events_str += " ";
        events_str += "-e " + m_cfg.events[i];
    }

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "%s record -d %s -c %d -p %d -o - 2>/dev/null | "
        "%s script -i - --fields=time,addr 2>/dev/null",
        m_cfg.perf_bin.c_str(), events_str.c_str(), m_cfg.frequency, m_pid,
        m_cfg.perf_bin.c_str());

    log_verbose("Starting perf pipeline: %s", cmd);

    m_pipe = popen(cmd, "r");
    if (!m_pipe) {
        log_err("Failed to start perf pipeline");
        return;
    }

    m_running = true;
    char line[256];

    while (!m_stop_requested && fgets(line, sizeof(line), m_pipe)) {
        // Parse perf script --fields=time,addr output.
        // Format: "<timestamp>:     <hex_addr>"
        // Example: "7938690.927119:     7ffe60e8b0b0"

        // Find the colon separator
        char* colon = strchr(line, ':');
        if (!colon) continue;

        // Skip whitespace after colon
        char* addr_start = colon + 1;
        while (*addr_start == ' ' || *addr_start == '\t') addr_start++;

        if (!*addr_start || *addr_start == '\n') continue;

        uint64_t addr = strtoull(addr_start, nullptr, 16);

        // Filter: skip kernel addresses (above 0x7fff...) and NULL-ish
        if (addr < 0x1000) continue;
        if (addr >= 0xffff000000000000ULL) continue;

        // Page-align and deliver
        addr &= ~(0xFFFULL);
        cb(addr);
    }

    m_running = false;

    if (m_pipe) {
        pclose(m_pipe);
        m_pipe = nullptr;
    }

    log_verbose("Sampler stopped");
}

void Sampler::stop() {
    m_stop_requested = true;

    if (m_pipe) {
        pclose(m_pipe);
        m_pipe = nullptr;
    }

    m_running = false;
}
