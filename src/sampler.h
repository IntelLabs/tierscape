#pragma once

#include "config.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <sys/types.h>

// Callback invoked for every parsed sample.
//   time_ns: perf-reported timestamp (CLOCK_MONOTONIC ns) or 0 if absent.
//   addr:    page-aligned virtual address.
using SampleCallback = std::function<void(uint64_t time_ns, uint64_t addr)>;

// Spawns a `perf record -d ... | perf script` pipeline as a child
// process group, then streams parsed addresses to the callback until
// stop() is called or the pipe closes.
class Sampler {
public:
    Sampler(const Config& cfg, pid_t target_pid);
    ~Sampler();

    // Run blocks until the pipeline EOFs or stop() is called.
    // Safe to call from a dedicated thread.
    void run(SampleCallback cb);

    // Async-signal-safe-ish: requests stop and signals the child group.
    // May be called from any thread.
    void stop();

    bool is_running() const { return m_running.load(); }

    // Per-process samples seen so far (atomic; debug only).
    uint64_t samples_seen() const { return m_samples_seen.load(); }

private:
    bool spawn();      // fork+exec; sets m_pipe_fd and m_child_pgid
    void shutdown_child();

    const Config& m_cfg;
    pid_t m_target_pid;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stop_requested{false};
    std::atomic<uint64_t> m_samples_seen{0};

    int   m_pipe_fd    = -1;   // read end of the child stdout
    pid_t m_child_pgid = -1;   // session/PGID of the shell child
};
