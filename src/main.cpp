#include "config.h"
#include "sanity.h"
#include "sampler.h"
#include "region.h"
#include "classifier.h"
#include "migrator.h"
#include "util.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>

static std::atomic<bool> g_running{true};
static Config g_cfg;

static void signal_handler(int sig) {
    (void)sig;
    g_running = false;
}

static void print_usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [OPTIONS] -p <PID>\n"
        "       %s [OPTIONS] -- <command> [args...]\n"
        "\n"
        "OPTIONS:\n"
        "  -p, --pid <PID>         Target process PID\n"
        "  -c, --config <path>     TOML config file (default: /etc/tierscaped.toml)\n"
        "      --hot-node <N>      NUMA node for hot tier\n"
        "      --cold-node <N>     NUMA node for cold tier\n"
        "      --hot-pct <float>   Hot percentile threshold (default: 25.0)\n"
        "      --freq <int>        PEBS sampling frequency (default: 10000)\n"
        "      --threads <int>     Migration threads (default: 2)\n"
        "      --window <int>      Window size in seconds (default: 20)\n"
        "      --region-size <sz>  Region size, e.g. 2M (default: 2M)\n"
        "      --max-pages <int>   Max pages per window (default: 5000000)\n"
        "  -v, --verbose           Verbose logging\n"
        "  -f, --foreground        Don't daemonize\n"
        "      --dry-run           Profile only, no migration\n"
        "      --pidfile <path>    PID file path (default: /tmp/tierscaped.pid)\n"
        "      --perf <path>       Path to perf binary\n"
        "  -h, --help              Show this help\n",
        prog, prog);
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
        {"help",        no_argument,       nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    // First, find if there's a "--" separator for launch command
    int dashdash_pos = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            dashdash_pos = i;
            break;
        }
    }

    // If -- found, limit option parsing to before it
    int parse_argc = (dashdash_pos > 0) ? dashdash_pos : argc;

    int opt;
    optind = 1;
    while ((opt = getopt_long(parse_argc, argv, "p:c:vfh", long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'p': cfg.target_pid = atoi(optarg); break;
            case 'c': /* config file loaded separately */ break;
            case 'v': cfg.verbose = true; break;
            case 'f': cfg.foreground = true; break;
            case 'h': print_usage(argv[0]); exit(0);
            case 1001: cfg.hot_node = atoi(optarg); break;
            case 1002: cfg.cold_node = atoi(optarg); break;
            case 1003: cfg.hot_percentile = atof(optarg); break;
            case 1004: cfg.frequency = atoi(optarg); break;
            case 1005: cfg.threads = atoi(optarg); break;
            case 1006: cfg.window_seconds = atoi(optarg); break;
            case 1007:
                cfg.region_size_str = optarg;
                cfg.region_size_bytes = parse_size(optarg);
                break;
            case 1008: cfg.max_pages_per_window = atoi(optarg); break;
            case 1009: cfg.dry_run = true; break;
            case 1010: cfg.pidfile = optarg; break;
            case 1011: cfg.perf_bin = optarg; break;
            default: return -1;
        }
    }

    // Collect launch command after --
    if (dashdash_pos > 0) {
        for (int i = dashdash_pos + 1; i < argc; i++) {
            cfg.launch_cmd.push_back(argv[i]);
        }
    }

    return 0;
}

static std::string find_config_file(int argc, char** argv) {
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            return argv[i + 1];
        }
    }
    // Check default locations
    if (access("tierscaped.toml", R_OK) == 0) return "tierscaped.toml";
    if (access("/etc/tierscaped.toml", R_OK) == 0) return "/etc/tierscaped.toml";
    return "";
}

static pid_t launch_child(const std::vector<std::string>& cmd) {
    pid_t pid = fork();
    if (pid < 0) {
        log_err("fork() failed");
        return -1;
    }
    if (pid == 0) {
        // Child: exec the command
        std::vector<char*> args;
        for (const auto& s : cmd) {
            args.push_back(const_cast<char*>(s.c_str()));
        }
        args.push_back(nullptr);
        execvp(args[0], args.data());
        perror("execvp");
        _exit(127);
    }
    return pid;
}

static int daemonize() {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);  // parent exits

    setsid();

    pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) _exit(0);  // first child exits

    // Redirect stdio to /dev/null
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    // Keep stderr if verbose, otherwise redirect
    if (!g_cfg.verbose && g_cfg.log_file.empty()) {
        freopen("/dev/null", "w", stderr);
    } else if (!g_cfg.log_file.empty()) {
        freopen(g_cfg.log_file.c_str(), "a", stderr);
    }

    return 0;
}

int main(int argc, char** argv) {
    // Load config file first (if exists), then CLI overrides
    std::string config_path = find_config_file(argc, argv);
    if (!config_path.empty()) {
        config_load(g_cfg, config_path);
    }

    // Parse CLI (overrides config file values)
    if (parse_args(argc, argv, g_cfg) != 0) {
        print_usage(argv[0]);
        return 1;
    }

    log_set_verbose(g_cfg.verbose);

    // Validate: must have either -p or -- command
    if (g_cfg.target_pid <= 0 && g_cfg.launch_cmd.empty()) {
        log_err("Must specify -p <PID> or -- <command>");
        print_usage(argv[0]);
        return 1;
    }

    // If launching a command, fork it now (before daemonizing)
    if (!g_cfg.launch_cmd.empty()) {
        pid_t child = launch_child(g_cfg.launch_cmd);
        if (child <= 0) {
            log_err("Failed to launch child process");
            return 1;
        }
        g_cfg.target_pid = child;
        log_info("Launched target process: PID %d", child);
        // Give the child a moment to start
        usleep(500000);
    }

    // Run sanity checks before daemonizing
    if (sanity_check_all(g_cfg) != 0) {
        log_err("Sanity checks failed. Exiting.");
        return 1;
    }

    // Daemonize (unless --foreground)
    if (!g_cfg.foreground) {
        if (daemonize() != 0) {
            log_err("Daemonization failed");
            return 1;
        }
    }

    // Write PID file
    if (write_pidfile(g_cfg.pidfile.c_str()) != 0) {
        log_warn("Could not write pidfile: %s", g_cfg.pidfile.c_str());
    }

    // Setup signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);

    log_info("tierscaped started: target PID=%d, hot_node=%d, cold_node=%d, "
             "window=%ds, hot_pct=%.1f, threads=%d, region_size=%s",
             g_cfg.target_pid, g_cfg.hot_node, g_cfg.cold_node,
             g_cfg.window_seconds, g_cfg.hot_percentile,
             g_cfg.threads, g_cfg.region_size_str.c_str());

    // Initialize components
    RegionManager region_mgr(g_cfg.region_size_bytes);
    std::mutex sample_mutex;

    // Start sampler thread
    Sampler sampler(g_cfg, g_cfg.target_pid);
    std::thread sampler_thread([&]() {
        sampler.run([&](uint64_t addr) {
            std::lock_guard<std::mutex> lock(sample_mutex);
            region_mgr.add_sample(addr);
        });
    });

    // Main window loop
    int window_count = 0;
    while (g_running && is_process_running(g_cfg.target_pid)) {
        // Sleep for window duration
        for (int i = 0; i < g_cfg.window_seconds && g_running; i++) {
            sleep(1);
            if (!is_process_running(g_cfg.target_pid)) break;
        }

        if (!g_running || !is_process_running(g_cfg.target_pid)) break;

        window_count++;

        // Snapshot regions under lock
        std::vector<Region> snapshot;
        {
            std::lock_guard<std::mutex> lock(sample_mutex);
            snapshot = region_mgr.regions();
            region_mgr.reset_hotness();
        }

        if (snapshot.empty()) {
            log_verbose("Window %d: no samples collected", window_count);
            continue;
        }

        log_verbose("Window %d: %zu regions with data", window_count, snapshot.size());

        // Classify hot/cold
        classify_regions(snapshot, g_cfg.hot_percentile,
                         g_cfg.hot_node, g_cfg.cold_node);

        // Migrate (unless dry-run)
        if (!g_cfg.dry_run) {
            int time_budget = (int)(g_cfg.window_seconds * 0.9);
            MigrateStats stats = migrate_regions(
                snapshot, g_cfg.region_size_bytes, g_cfg.target_pid,
                g_cfg.threads, g_cfg.max_pages_per_window, time_budget);

            log_info("Window %d: moved %lu pages (%lu regions), "
                     "%lu in-place, %lu errors",
                     window_count, stats.pages_moved, stats.regions_moved,
                     stats.already_in_place, stats.errors);

            // Update region manager with new current_node info
            {
                std::lock_guard<std::mutex> lock(sample_mutex);
                auto& real_regions = region_mgr.regions();
                for (size_t i = 0; i < snapshot.size() && i < real_regions.size(); i++) {
                    if (snapshot[i].start_addr == real_regions[i].start_addr) {
                        real_regions[i].current_node = snapshot[i].current_node;
                    }
                }
            }
        } else {
            log_info("Window %d: %zu regions (dry-run, no migration)",
                     window_count, snapshot.size());
        }
    }

    // Cleanup
    log_info("Target process exited or stop signal received. Shutting down.");
    sampler.stop();

    if (sampler_thread.joinable()) {
        sampler_thread.join();
    }

    remove_pidfile(g_cfg.pidfile.c_str());

    // If we launched the child, wait for it
    if (!g_cfg.launch_cmd.empty()) {
        int status;
        waitpid(g_cfg.target_pid, &status, WNOHANG);
    }

    log_info("tierscaped exited cleanly after %d windows.", window_count);
    return 0;
}
