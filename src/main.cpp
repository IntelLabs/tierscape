#include "classifier.h"
#include "config.h"
#include "migrator.h"
#include "proc_maps.h"
#include "region.h"
#include "sampler.h"
#include "sanity.h"
#include "util.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>

static std::atomic<bool> g_running{true};
static std::atomic<bool> g_child_exited{false};
static Config g_cfg;

static void on_termish(int) {
    g_running.store(false, std::memory_order_relaxed);
}

static void on_sigchld(int) {
    // Just flag it; main loop polls is_process_running() too.
    g_child_exited.store(true, std::memory_order_relaxed);
}

static void install_signal_handlers() {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_termish;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT,  &sa, nullptr);

    sa.sa_handler = on_sigchld;
    sa.sa_flags   = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, nullptr);

    // We never want SIGPIPE to kill us (perf pipeline may close abruptly).
    sa.sa_handler = SIG_IGN;
    sa.sa_flags   = 0;
    sigaction(SIGPIPE, &sa, nullptr);
}

static void print_usage(const char* prog) {
    std::fprintf(stderr,
        "Usage: %s [OPTIONS] -p <PID>\n"
        "       %s [OPTIONS] -- <command> [args...]\n"
        "\n"
        "OPTIONS:\n"
        "  -p, --pid <PID>         Target process PID\n"
        "  -c, --config <path>     TOML config file\n"
        "      --hot-node <N>      NUMA node for hot tier\n"
        "      --cold-node <N>     NUMA node for cold tier\n"
        "      --hot-pct <float>   Hotness percentile threshold (default 25)\n"
        "                          Regions below this percentile are demoted.\n"
        "      --freq <int>        PEBS sampling period (perf -c) (default 10000)\n"
        "      --threads <int>     Migration worker threads (default 2)\n"
        "      --window <int>      Profiling window in seconds (default 20)\n"
        "      --region-size <sz>  Region size, e.g. 2M (default 2M)\n"
        "      --max-pages <int>   Max pages migrated per window (default 5000000)\n"
        "      --max-idle <int>    Evict regions idle for this many windows (default 10)\n"
        "      --dump-file <path>  Dump raw samples as 'time_ms addr' to file\n"
        "  -v, --verbose           Verbose logging\n"
        "  -f, --foreground        Don't daemonize\n"
        "      --dry-run           Profile only, no migration\n"
        "      --pidfile <path>    PID file (default /tmp/tierscaped.pid)\n"
        "      --perf <path>       perf binary path\n"
        "  -h, --help              Show this help\n",
        prog, prog);
}

static bool require_long(const char* s, long lo, long hi, long& out,
                         const char* name) {
    long v;
    if (!parse_int(s, v) || v < lo || v > hi) {
        log_err("Invalid value for %s: '%s' (expected %ld..%ld)",
                name, s, lo, hi);
        return false;
    }
    out = v;
    return true;
}

static bool require_double(const char* s, double lo, double hi, double& out,
                           const char* name) {
    double v;
    if (!parse_float(s, v) || v < lo || v > hi) {
        log_err("Invalid value for %s: '%s' (expected %.2f..%.2f)",
                name, s, lo, hi);
        return false;
    }
    out = v;
    return true;
}

static int parse_args(int argc, char** argv, Config& cfg) {
    static struct option long_opts[] = {
        {"pid",         required_argument, nullptr, 'p'},
        {"config",      required_argument, nullptr, 'c'},
        {"hot-node",    required_argument, nullptr, 1001},
        {"cold-node",   required_argument, nullptr, 1002},
        {"hot-pct",     required_argument, nullptr, 1003},
        {"freq",        required_argument, nullptr, 1004},
        {"threads",     required_argument, nullptr, 1005},
        {"window",      required_argument, nullptr, 1006},
        {"region-size", required_argument, nullptr, 1007},
        {"max-pages",   required_argument, nullptr, 1008},
        {"verbose",     no_argument,       nullptr, 'v'},
        {"foreground",  no_argument,       nullptr, 'f'},
        {"dry-run",     no_argument,       nullptr, 1009},
        {"pidfile",     required_argument, nullptr, 1010},
        {"perf",        required_argument, nullptr, 1011},
        {"max-idle",    required_argument, nullptr, 1012},
        {"dump-file",   required_argument, nullptr, 1013},
        {"help",        no_argument,       nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    int dashdash_pos = -1;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--") == 0) { dashdash_pos = i; break; }
    }
    int parse_argc = (dashdash_pos > 0) ? dashdash_pos : argc;

    long lv; double dv;
    int opt;
    optind = 1;
    while ((opt = getopt_long(parse_argc, argv, "p:c:vfh", long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'p':
                if (!require_long(optarg, 1, INT32_MAX, lv, "pid")) return -1;
                cfg.target_pid = static_cast<pid_t>(lv);
                break;
            case 'c': /* loaded separately */ break;
            case 'v': cfg.verbose    = true; break;
            case 'f': cfg.foreground = true; break;
            case 'h': print_usage(argv[0]); std::exit(0);
            case 1001:
                if (!require_long(optarg, 0, 1023, lv, "hot-node")) return -1;
                cfg.hot_node = static_cast<int>(lv); break;
            case 1002:
                if (!require_long(optarg, 0, 1023, lv, "cold-node")) return -1;
                cfg.cold_node = static_cast<int>(lv); break;
            case 1003:
                if (!require_double(optarg, 0.0, 100.0, dv, "hot-pct")) return -1;
                cfg.hot_percentile = static_cast<float>(dv); break;
            case 1004:
                if (!require_long(optarg, 1, INT32_MAX, lv, "freq")) return -1;
                cfg.frequency = static_cast<int>(lv); break;
            case 1005:
                if (!require_long(optarg, 1, 1024, lv, "threads")) return -1;
                cfg.threads = static_cast<int>(lv); break;
            case 1006:
                if (!require_long(optarg, 1, 86400, lv, "window")) return -1;
                cfg.window_seconds = static_cast<int>(lv); break;
            case 1007: {
                uint64_t sz = parse_size(optarg);
                if (sz == 0) {
                    log_err("Invalid region-size: '%s'", optarg);
                    return -1;
                }
                cfg.region_size_str   = optarg;
                cfg.region_size_bytes = sz;
                break;
            }
            case 1008:
                if (!require_long(optarg, 1, INT64_MAX, lv, "max-pages")) return -1;
                cfg.max_pages_per_window = static_cast<uint64_t>(lv); break;
            case 1009: cfg.dry_run = true; break;
            case 1010: cfg.pidfile  = optarg; break;
            case 1011: cfg.perf_bin = optarg; break;
            case 1012:
                if (!require_long(optarg, 1, INT32_MAX, lv, "max-idle")) return -1;
                cfg.max_idle_windows = static_cast<int>(lv); break;
            case 1013: cfg.dump_file = optarg; break;
            default: return -1;
        }
    }

    if (dashdash_pos > 0) {
        for (int i = dashdash_pos + 1; i < argc; ++i) {
            cfg.launch_cmd.emplace_back(argv[i]);
        }
    }
    return 0;
}

static std::string find_config_file(int argc, char** argv) {
    for (int i = 1; i < argc - 1; ++i) {
        if (std::strcmp(argv[i], "-c") == 0 ||
            std::strcmp(argv[i], "--config") == 0) {
            return argv[i + 1];
        }
    }
    if (access("tierscaped.toml", R_OK) == 0) return "tierscaped.toml";
    if (access("/etc/tierscaped.toml", R_OK) == 0) return "/etc/tierscaped.toml";
    return "";
}

static pid_t launch_child(const std::vector<std::string>& cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        log_err("fork() failed: %s", std::strerror(errno));
        return -1;
    }
    if (pid == 0) {
        std::vector<char*> args;
        args.reserve(cmd.size() + 1);
        for (const auto& s : cmd) args.push_back(const_cast<char*>(s.c_str()));
        args.push_back(nullptr);
        execvp(args[0], args.data());
        std::perror("execvp");
        _exit(127);
    }
    return pid;
}

static int daemonize() {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);

    setsid();

    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);

    if (!std::freopen("/dev/null", "r", stdin))  { /* ignore */ }
    if (!std::freopen("/dev/null", "w", stdout)) { /* ignore */ }
    if (!g_cfg.verbose && g_cfg.log_file.empty()) {
        if (!std::freopen("/dev/null", "w", stderr)) { /* ignore */ }
    } else if (!g_cfg.log_file.empty()) {
        if (!std::freopen(g_cfg.log_file.c_str(), "a", stderr)) { /* ignore */ }
    }
    return 0;
}

int main(int argc, char** argv) {
    // Load config first; CLI overrides.
    std::string config_path = find_config_file(argc, argv);
    if (!config_path.empty()) {
        config_load(g_cfg, config_path);
    }

    if (parse_args(argc, argv, g_cfg) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    log_set_verbose(g_cfg.verbose);

    if (g_cfg.target_pid <= 0 && g_cfg.launch_cmd.empty()) {
        log_err("Must specify -p <PID> or -- <command>");
        print_usage(argv[0]);
        return 1;
    }

    install_signal_handlers();

    if (!g_cfg.launch_cmd.empty()) {
        pid_t child = launch_child(g_cfg.launch_cmd);
        if (child <= 0) {
            log_err("Failed to launch child");
            return 1;
        }
        g_cfg.target_pid = child;
        log_info("Launched target PID %d", child);
        // Wait briefly for the child to exec and produce its maps file.
        for (int i = 0; i < 50; ++i) {
            if (can_read_proc(child)) break;
            struct timespec ts = {0, 20 * 1000 * 1000};  // 20 ms
            nanosleep(&ts, nullptr);
        }
    }

    if (sanity_check_all(g_cfg) != 0) {
        log_err("Sanity checks failed");
        return 1;
    }

    if (!g_cfg.foreground) {
        if (daemonize() != 0) {
            log_err("Daemonization failed");
            return 1;
        }
    }

    if (write_pidfile(g_cfg.pidfile.c_str()) != 0) {
        log_warn("Could not write pidfile: %s", g_cfg.pidfile.c_str());
    }

    log_info("tierscaped started: target_pid=%d, hot=%d, cold=%d, "
             "window=%ds, hot_pct=%.1f, threads=%d, region=%s",
             g_cfg.target_pid, g_cfg.hot_node, g_cfg.cold_node,
             g_cfg.window_seconds, g_cfg.hot_percentile,
             g_cfg.threads, g_cfg.region_size_str.c_str());

    RegionManager region_mgr(g_cfg.region_size_bytes);
    Sampler sampler(g_cfg, g_cfg.target_pid);

    // Optional sample dump file for offline analysis.
    FILE* dump_fp = nullptr;
    if (!g_cfg.dump_file.empty()) {
        dump_fp = std::fopen(g_cfg.dump_file.c_str(), "w");
        if (!dump_fp) {
            log_warn("Cannot open dump file: %s", g_cfg.dump_file.c_str());
        } else {
            std::fprintf(dump_fp, "time_ms addr\n");
        }
    }
    auto t_start = std::chrono::steady_clock::now();

    std::thread sampler_thread([&]() {
        sampler.run([&](uint64_t addr) {
            region_mgr.add_sample(addr);
            if (dump_fp) {
                auto now = std::chrono::steady_clock::now();
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - t_start).count();
                std::fprintf(dump_fp, "%ld 0x%lx\n", (long)ms, (unsigned long)addr);
            }
        });
    });

    int window_count = 0;
    while (g_running.load(std::memory_order_relaxed) &&
           is_process_running(g_cfg.target_pid)) {
        // Sleep for window_seconds, but bail on signal / child death.
        for (int i = 0; i < g_cfg.window_seconds; ++i) {
            if (!g_running.load(std::memory_order_relaxed)) break;
            if (!is_process_running(g_cfg.target_pid)) break;
            sleep(1);
        }
        if (!g_running.load(std::memory_order_relaxed) ||
            !is_process_running(g_cfg.target_pid)) break;

        ++window_count;

        std::vector<Region> snap = region_mgr.snapshot_and_swap();
        if (snap.empty()) {
            log_verbose("Window %d: no samples", window_count);
            region_mgr.evict_idle(g_cfg.max_idle_windows);
            continue;
        }
        log_verbose("Window %d: %zu sampled regions (%zu tracked)",
                    window_count, snap.size(), region_mgr.tracked_count());

        classify_regions(snap, g_cfg.hot_percentile,
                         g_cfg.hot_node, g_cfg.cold_node);

        if (!g_cfg.dry_run) {
            std::vector<Vma> vmas = read_proc_maps(g_cfg.target_pid);
            int budget = static_cast<int>(g_cfg.window_seconds * 0.9);
            MigrateStats st = migrate_regions(
                snap, g_cfg.region_size_bytes, g_cfg.target_pid,
                g_cfg.threads, g_cfg.max_pages_per_window,
                budget, vmas, g_cfg.hot_node, g_cfg.cold_node);

            log_info("Window %d: moved %lu pages (%lu promoted, %lu demoted), "
                     "%lu regions, %lu in-place, %lu errors, %lu skipped",
                     window_count, st.pages_moved, st.pages_promoted,
                     st.pages_demoted, st.regions_moved,
                     st.already_in_place, st.errors, st.skipped_no_vma);

            // Address-based update of persistent current_node — robust
            // against intervening sampler-thread inserts.
            for (const auto& r : snap) {
                if (r.current_node >= 0) {
                    region_mgr.update_current_node(r.start_addr, r.current_node);
                }
            }
        } else {
            log_info("Window %d: %zu regions (dry-run)", window_count, snap.size());
        }

        size_t evicted = region_mgr.evict_idle(g_cfg.max_idle_windows);
        if (evicted) {
            log_verbose("Evicted %zu idle regions", evicted);
        }
    }

    log_info("Shutting down: target exited or stop signal received");
    sampler.stop();
    if (sampler_thread.joinable()) sampler_thread.join();

    if (dump_fp) {
        std::fclose(dump_fp);
        log_info("Sample dump written: %s", g_cfg.dump_file.c_str());
    }

    remove_pidfile(g_cfg.pidfile.c_str());

    // Reap any launched child cleanly.
    if (!g_cfg.launch_cmd.empty() && g_cfg.target_pid > 0) {
        // If still alive, leave it alone; otherwise reap to avoid zombie.
        int status;
        waitpid(g_cfg.target_pid, &status, WNOHANG);
    }

    log_info("tierscaped exited cleanly after %d windows", window_count);
    return 0;
}
