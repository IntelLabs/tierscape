// tierscaped — NUMA tiering daemon.
//
// High-level orchestration. The interesting bits live in:
//   cli.{h,cpp}     : argv -> Config (with TOML fallback)
//   sanity.{h,cpp}  : pre-flight checks (NUMA nodes, perf binary, etc.)
//   runtime.{h,cpp} : signals, daemonize, launch_child, the window loop,
//                     startup & summary logging
//   sampler.{h,cpp} : perf record|script -> Sampler callbacks
//   region.{h,cpp}  : sample bucketing + per-window snapshots
//   classifier.h    : assign each region a target_node by percentile
//   migrator.h      : move_pages() workers, clipped to anon VMAs
//
// This file just sequences them. Read top-to-bottom.

#include "cli.h"
#include "config.h"
#include "runtime.h"
#include "sanity.h"
#include "util.h"

int main(int argc, char** argv) {
    Config cfg;

    // 1. Parse CLI (TOML file first, flags override).
    if (cli_load(argc, argv, cfg) != 0) return 1;
    log_set_verbose(cfg.verbose);

    // 2. Install signal handlers, then launch a target if the user
    //    gave us `-- <cmd>` instead of `-p <pid>`.
    install_signal_handlers();
    if (launch_target_if_needed(cfg) != 0) return 1;

    // 3. Verify we can do what we're about to do (NUMA nodes exist,
    //    perf is callable, /proc/<pid>/maps is readable, ...).
    if (sanity_check_all(cfg) != 0) {
        log_err("Sanity checks failed");
        return 1;
    }

    // 4. Detach if requested, then publish our PID for systemd & friends.
    if (!cfg.foreground && daemonize(cfg) != 0) {
        log_err("Daemonization failed");
        return 1;
    }
    if (write_pidfile(cfg.pidfile.c_str()) != 0) {
        log_warn("Could not write pidfile: %s", cfg.pidfile.c_str());
    }

    // 5. Log everything that affects behavior — handy for `tail` debugging.
    print_effective_config(cfg);

    // 6. Profile -> classify -> migrate, once per window, until done.
    RunStats stats = run_loop(cfg);

    // 7. Clean up and print the run summary.
    remove_pidfile(cfg.pidfile.c_str());
    print_run_summary(cfg, stats);
    return 0;
}
