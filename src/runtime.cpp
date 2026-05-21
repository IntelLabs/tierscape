#include "runtime.h"

#include "classifier.h"
#include "migrator.h"
#include "proc_maps.h"
#include "region.h"
#include "sampler.h"
#include "util.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

// ──────────────────────────────────────────────────────────────────────────────
// Process lifecycle: signals, child launch, daemonization.
// ──────────────────────────────────────────────────────────────────────────────

std::atomic<bool> g_running{true};

static void on_termish(int) {
    g_running.store(false, std::memory_order_relaxed);
}

static void on_sigchld(int) {
    // Nothing to do here — the main loop already polls
    // is_process_running() once per second. Handler exists only so
    // SIGCHLD doesn't terminate us by default.
}

void install_signal_handlers() {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);

    sa.sa_handler = on_termish;
    sa.sa_flags   = SA_RESTART;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT,  &sa, nullptr);

    sa.sa_handler = on_sigchld;
    sa.sa_flags   = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, nullptr);

    // perf | script pipeline can close on us — don't let SIGPIPE kill us.
    sa.sa_handler = SIG_IGN;
    sa.sa_flags   = 0;
    sigaction(SIGPIPE, &sa, nullptr);
}

int launch_target_if_needed(Config& cfg) {
    if (cfg.launch_cmd.empty()) return 0;

    pid_t pid = fork();
    if (pid < 0) {
        log_err("fork() failed: %s", std::strerror(errno));
        return -1;
    }
    if (pid == 0) {
        std::vector<char*> args;
        args.reserve(cfg.launch_cmd.size() + 1);
        for (const auto& s : cfg.launch_cmd) {
            args.push_back(const_cast<char*>(s.c_str()));
        }
        args.push_back(nullptr);
        execvp(args[0], args.data());
        std::perror("execvp");
        _exit(127);
    }

    cfg.target_pid = pid;
    log_info("Launched target PID %d", pid);

    // Wait briefly for the child to exec and produce its maps file.
    for (int i = 0; i < 50; ++i) {
        if (can_read_proc(pid)) break;
        struct timespec ts = {0, 20 * 1000 * 1000};  // 20 ms
        nanosleep(&ts, nullptr);
    }
    return 0;
}

int daemonize(const Config& cfg) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);

    setsid();

    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);

    if (!std::freopen("/dev/null", "r", stdin))  { /* ignore */ }
    if (!std::freopen("/dev/null", "w", stdout)) { /* ignore */ }
    if (!cfg.verbose && cfg.log_file.empty()) {
        if (!std::freopen("/dev/null", "w", stderr)) { /* ignore */ }
    } else if (!cfg.log_file.empty()) {
        if (!std::freopen(cfg.log_file.c_str(), "a", stderr)) { /* ignore */ }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// Configuration & summary logging.
// ──────────────────────────────────────────────────────────────────────────────

void print_effective_config(const Config& cfg) {
    log_info("tierscaped started: target_pid=%d, hot=%d, cold=%d, "
             "window=%ds, hot_pct=%.1f, threads=%d, region=%s",
             cfg.target_pid, cfg.hot_node, cfg.cold_node,
             cfg.window_seconds, cfg.hot_percentile,
             cfg.threads, cfg.region_size_str.c_str());

    log_info("=== Effective configuration ===");
    log_info("  [tiers]        hot_node=%d, cold_node=%d",
             cfg.hot_node, cfg.cold_node);
    log_info("  [sampling]     window_seconds=%d, frequency=%d, events=%zu",
             cfg.window_seconds, cfg.frequency, cfg.events.size());
    for (size_t i = 0; i < cfg.events.size(); ++i) {
        log_info("                 event[%zu]=%s", i, cfg.events[i].c_str());
    }
    log_info("  [classify]     hot_percentile=%.2f (top %.0f%% kept hot)",
             cfg.hot_percentile, 100.0f - cfg.hot_percentile);
    log_info("  [migration]    threads=%d, region_size=%s (%lu bytes), "
             "max_pages/window=%lu, max_idle_windows=%d",
             cfg.threads, cfg.region_size_str.c_str(),
             (unsigned long)cfg.region_size_bytes,
             (unsigned long)cfg.max_pages_per_window,
             cfg.max_idle_windows);
    log_info("  [daemon]       foreground=%d, dry_run=%d, perf=%s",
             cfg.foreground ? 1 : 0, cfg.dry_run ? 1 : 0,
             cfg.perf_bin.c_str());
    log_info("                 pidfile=%s, dump_file=%s",
             cfg.pidfile.c_str(),
             cfg.dump_file.empty() ? "<none>" : cfg.dump_file.c_str());
    log_info("================================");
}

void print_run_summary(const Config& cfg, const RunStats& s) {
    const double mb_per_page = 4096.0 / (1024.0 * 1024.0);
    double moved_mb    = s.pages_moved    * mb_per_page;
    double promoted_mb = s.pages_promoted * mb_per_page;
    double demoted_mb  = s.pages_demoted  * mb_per_page;
    double completion_pct = (s.planned_migrations > 0)
        ? (100.0 * static_cast<double>(s.regions_moved) / s.planned_migrations)
        : 100.0;

    log_info("=== Run summary ===");
    log_info("  duration            : %.1f s (%d windows, %d empty)",
             s.duration_seconds, s.windows, s.empty_windows);
    log_info("  peak tracked        : %zu regions (~%.0f MB at %s)",
             s.peak_tracked,
             s.peak_tracked * (cfg.region_size_bytes / 1024.0 / 1024.0),
             cfg.region_size_str.c_str());
    log_info("  data moved          : %.0f MB total (%.0f promoted, %.0f demoted)",
             moved_mb, promoted_mb, demoted_mb);
    log_info("  pages moved         : %lu (%lu promoted, %lu demoted, %lu errors)",
             (unsigned long)s.pages_moved, (unsigned long)s.pages_promoted,
             (unsigned long)s.pages_demoted, (unsigned long)s.errors);
    log_info("  avg per window      : %.1f MB/window, %.1f MB/s",
             s.windows > 0 ? moved_mb / s.windows : 0.0,
             s.duration_seconds > 0 ? moved_mb / s.duration_seconds : 0.0);
    log_info("  migration completion: %lu / %lu regions planned (%.1f%%) → "
             "%d incomplete, %d capped windows",
             (unsigned long)s.regions_moved, (unsigned long)s.planned_migrations,
             completion_pct, s.incomplete_windows, s.capped_windows);
    if (s.capped_windows > 0) {
        log_info("  NOTE: %d window(s) hit max_pages_per_window=%lu — "
                 "consider raising the cap or shortening the window",
                 s.capped_windows, (unsigned long)cfg.max_pages_per_window);
    }
    log_info("===================");
}

// ──────────────────────────────────────────────────────────────────────────────
// The main profile + classify + migrate loop.
// ──────────────────────────────────────────────────────────────────────────────

namespace {

// Open the sample dump file (full-buffered) and write the header.
// Returns nullptr if `path` is empty or the file cannot be opened.
FILE* open_dump_file(const std::string& path) {
    if (path.empty()) return nullptr;
    FILE* fp = std::fopen(path.c_str(), "w");
    if (!fp) {
        log_warn("Cannot open dump file: %s", path.c_str());
        return nullptr;
    }
    std::setvbuf(fp, nullptr, _IOFBF, 1 << 16);
    std::fprintf(fp, "time_ms addr\n");
    return fp;
}

// Wait up to `window_seconds`, breaking early on signal/target death.
// Returns true if the loop should continue, false to break out.
bool wait_for_window(int window_seconds, pid_t target_pid) {
    for (int i = 0; i < window_seconds; ++i) {
        if (!g_running.load(std::memory_order_relaxed)) return false;
        if (!is_process_running(target_pid))            return false;
        sleep(1);
    }
    return g_running.load(std::memory_order_relaxed) &&
           is_process_running(target_pid);
}

// Process a single profile window: classify all tracked regions, then
// migrate where target != current. Updates `s` in place.
void process_window(const Config& cfg, RegionManager& region_mgr,
                    int window_count, RunStats& s) {
    std::vector<Region> snap = region_mgr.snapshot_and_swap();
    if (snap.empty()) {
        log_verbose("Window %d: no regions tracked yet", window_count);
        ++s.empty_windows;
        region_mgr.evict_idle(cfg.max_idle_windows);
        return;
    }
    if (snap.size() > s.peak_tracked) s.peak_tracked = snap.size();

    size_t sampled_now = 0;
    for (const auto& r : snap) if (r.hotness > 0) ++sampled_now;
    log_verbose("Window %d: %zu sampled / %zu tracked regions classified",
                window_count, sampled_now, snap.size());

    classify_regions(snap, cfg.hot_percentile, cfg.hot_node, cfg.cold_node);

    if (cfg.dry_run) {
        log_info("Window %d: %zu regions (dry-run)", window_count, snap.size());
        size_t evicted = region_mgr.evict_idle(cfg.max_idle_windows);
        if (evicted) log_verbose("Evicted %zu idle regions", evicted);
        return;
    }

    std::vector<Vma> vmas = read_proc_maps(cfg.target_pid);
    int budget = static_cast<int>(cfg.window_seconds * 0.9);

    // Count planned migrations (target != current) before calling.
    uint64_t planned = 0;
    for (const auto& r : snap) {
        if (r.target_node >= 0 && r.target_node != r.current_node) ++planned;
    }

    MigrateStats st = migrate_regions(
        snap, cfg.region_size_bytes, cfg.target_pid,
        cfg.threads, cfg.max_pages_per_window,
        budget, vmas, cfg.hot_node, cfg.cold_node);

    log_info("Window %d: moved %lu pages (%lu promoted, %lu demoted), "
             "%lu/%lu regions, %lu in-place, %lu errors, %lu skipped",
             window_count, st.pages_moved, st.pages_promoted,
             st.pages_demoted, st.regions_moved, planned,
             st.already_in_place, st.errors, st.skipped_no_vma);

    // Roll per-window stats into the run aggregate.
    s.pages_moved        += st.pages_moved;
    s.pages_promoted     += st.pages_promoted;
    s.pages_demoted      += st.pages_demoted;
    s.regions_moved      += st.regions_moved;
    s.errors             += st.errors;
    s.planned_migrations += planned;
    if (st.pages_moved >= cfg.max_pages_per_window) ++s.capped_windows;
    // Non-anon regions (libc text, stack) can never migrate, so subtract
    // them before deciding whether the window was "incomplete".
    uint64_t migratable = (planned > st.skipped_no_vma)
                          ? (planned - st.skipped_no_vma) : 0;
    if (migratable > 0 && st.regions_moved < migratable) ++s.incomplete_windows;

    // Persist current_node back into RegionManager. Address-based update
    // is robust against intervening sampler-thread inserts.
    for (const auto& r : snap) {
        if (r.current_node >= 0) {
            region_mgr.update_current_node(r.start_addr, r.current_node);
        }
    }

    size_t evicted = region_mgr.evict_idle(cfg.max_idle_windows);
    if (evicted) log_verbose("Evicted %zu idle regions", evicted);
}

}  // namespace

RunStats run_loop(const Config& cfg) {
    RunStats s;
    RegionManager region_mgr(cfg.region_size_bytes);
    Sampler       sampler(cfg, cfg.target_pid);

    FILE* dump_fp = open_dump_file(cfg.dump_file);
    auto  t_start = std::chrono::steady_clock::now();

    // Anchor for perf-reported timestamps (CLOCK_MONOTONIC ns since
    // boot). The dump file column "time_ms" is ms-since-first-sample
    // so the timeline aligns with the daemon's per-window log.
    std::atomic<uint64_t> first_perf_ns{0};

    std::thread sampler_thread([&]() {
        sampler.run([&](uint64_t time_ns, uint64_t addr) {
            region_mgr.add_sample(addr);
            if (!dump_fp) return;
            long ms;
            if (time_ns != 0) {
                uint64_t base = first_perf_ns.load(std::memory_order_relaxed);
                if (base == 0) {
                    first_perf_ns.compare_exchange_strong(
                        base, time_ns, std::memory_order_relaxed);
                    base = first_perf_ns.load(std::memory_order_relaxed);
                }
                uint64_t rel = (time_ns >= base) ? (time_ns - base) : 0;
                ms = static_cast<long>(rel / 1'000'000ULL);
            } else {
                auto now = std::chrono::steady_clock::now();
                ms = static_cast<long>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - t_start).count());
            }
            std::fprintf(dump_fp, "%ld 0x%lx\n", ms, (unsigned long)addr);
        });
    });

    int window = 0;
    while (g_running.load(std::memory_order_relaxed) &&
           is_process_running(cfg.target_pid)) {
        if (!wait_for_window(cfg.window_seconds, cfg.target_pid)) break;
        process_window(cfg, region_mgr, ++window, s);
    }
    s.windows = window;
    s.duration_seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start)
            .count();

    log_info("Shutting down: target exited or stop signal received");
    sampler.stop();
    if (sampler_thread.joinable()) sampler_thread.join();

    if (dump_fp) {
        std::fclose(dump_fp);
        log_info("Sample dump written: %s", cfg.dump_file.c_str());
    }

    // Reap any launched child cleanly. Only meaningful when we did NOT
    // daemonize — after daemonize() the child is reparented to init.
    if (!cfg.launch_cmd.empty() && cfg.target_pid > 0 && cfg.foreground) {
        int status;
        waitpid(cfg.target_pid, &status, WNOHANG);
    }

    log_info("tierscaped exited cleanly after %d windows", window);
    return s;
}
