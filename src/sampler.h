#pragma once

#include "config.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <sys/types.h>

// Callback invoked for each parsed PEBS sample address
using SampleCallback = std::function<void(uint64_t addr)>;

class Sampler {
public:
    Sampler(const Config& cfg, pid_t pid);
    ~Sampler();

    // Start the perf pipeline. Calls cb for each sample address.
    // Runs until stop() is called or the pipe closes.
    // This blocks — run in a dedicated thread.
    void run(SampleCallback cb);

    // Signal the sampler to stop
    void stop();

    bool is_running() const { return m_running.load(); }

private:
    const Config& m_cfg;
    pid_t m_pid;
    FILE* m_pipe = nullptr;
    pid_t m_perf_pid = -1;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop_requested{false};
};
