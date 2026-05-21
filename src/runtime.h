#pragma once

#include "config.h"

#include <atomic>
#include <cstdint>
#include <vector>

// ──────────────────────────────────────────────────────────────────────────────
// Runtime support for the tierscaped daemon.
//
// This header collects everything `main()` needs to drive the daemon's
// lifecycle, with the goal of keeping main.cpp a short, top-to-bottom story:
//
//   1. parse CLI + load config         (cli.h)
//   2. install signals + launch target (this file)
//   3. daemonize / write pidfile       (this file)
//   4. log the effective configuration (this file)
//   5. run the profile + classify + migrate loop (this file)
//   6. log the run summary             (this file)
//
// Everything here is process-scoped and safe to call exactly once.
// ──────────────────────────────────────────────────────────────────────────────

// Global stop flag. Flipped to `false` by SIGINT/SIGTERM. The run loop
// polls it once per second and exits cleanly.
extern std::atomic<bool> g_running;

// Install SIGTERM/SIGINT (graceful stop), SIGCHLD (child reap), and
// SIGPIPE (ignore — perf pipeline may close abruptly).
void install_signal_handlers();

// Fork+exec the command in `cfg.launch_cmd`. Returns the child's PID
// (>0) on success or -1 on failure. Caller assigns it to
// cfg.target_pid. Waits briefly for /proc/<pid>/maps to appear.
// No-op (returns 0) when cfg.launch_cmd is empty.
int launch_target_if_needed(Config& cfg);

// Standard double-fork daemonization. Redirects stdio to /dev/null or
// to cfg.log_file when set. Returns 0 on success, -1 on failure.
int daemonize(const Config& cfg);

// Pretty-print every field of `cfg` at INFO level so a future operator
// can reproduce a run from the log alone.
void print_effective_config(const Config& cfg);

// Aggregate counters collected by run_loop() and consumed by
// print_run_summary().
struct RunStats {
    int      windows             = 0;   // total windows executed
    int      empty_windows       = 0;   // windows with no tracked regions
    int      capped_windows      = 0;   // hit max_pages_per_window
    int      incomplete_windows  = 0;   // regions_moved < migratable planned
    uint64_t pages_moved         = 0;
    uint64_t pages_promoted      = 0;
    uint64_t pages_demoted       = 0;
    uint64_t regions_moved       = 0;
    uint64_t planned_migrations  = 0;
    uint64_t errors              = 0;
    std::size_t peak_tracked     = 0;
    double   duration_seconds    = 0.0;
};

// Run the profile -> classify -> migrate loop until either the target
// exits or g_running flips to false. Returns aggregate statistics.
// Honors cfg.dry_run (skips migration), cfg.dump_file (raw samples),
// and cfg.window_seconds (loop cadence).
RunStats run_loop(const Config& cfg);

// Print the end-of-run summary block at INFO level.
void print_run_summary(const Config& cfg, const RunStats& stats);
