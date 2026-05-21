#include "cli.h"
#include "util.h"

#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <unistd.h>

namespace {

void print_usage(const char* prog) {
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
        "                          Regions below this percentile are demoted;\n"
        "                          top (100 - hot-pct)%% of the footprint stays hot.\n"
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

bool require_long(const char* s, long lo, long hi, long& out, const char* name) {
    long v;
    if (!parse_int(s, v) || v < lo || v > hi) {
        log_err("Invalid value for %s: '%s' (expected %ld..%ld)",
                name, s, lo, hi);
        return false;
    }
    out = v;
    return true;
}

bool require_double(const char* s, double lo, double hi, double& out,
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

template <class T>
bool opt_int(const char* s, T& dst, long lo, long hi, const char* name) {
    long v;
    if (!require_long(s, lo, hi, v, name)) return false;
    dst = static_cast<T>(v);
    return true;
}

bool opt_float(const char* s, float& dst, double lo, double hi, const char* name) {
    double v;
    if (!require_double(s, lo, hi, v, name)) return false;
    dst = static_cast<float>(v);
    return true;
}

// Scan argv for the config-file flag (any of: -c FILE, --config FILE,
// --config=FILE, -cFILE). Falls back to ./tierscaped.toml and
// /etc/tierscaped.toml when readable.
std::string find_config_file(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (i < argc - 1 &&
            (std::strcmp(argv[i], "-c") == 0 ||
             std::strcmp(argv[i], "--config") == 0)) {
            return argv[i + 1];
        }
        const char* prefix = "--config=";
        size_t plen = std::strlen(prefix);
        if (std::strncmp(argv[i], prefix, plen) == 0) {
            return argv[i] + plen;
        }
        if (argv[i][0] == '-' && argv[i][1] == 'c' &&
            argv[i][2] != '\0' && argv[i][2] != '-') {
            return argv[i] + 2;
        }
    }
    if (access("tierscaped.toml", R_OK) == 0) return "tierscaped.toml";
    if (access("/etc/tierscaped.toml", R_OK) == 0) return "/etc/tierscaped.toml";
    return "";
}

int parse_args(int argc, char** argv, Config& cfg) {
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

    // Args after `--` are forwarded as the target command to launch.
    int dashdash_pos = -1;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--") == 0) { dashdash_pos = i; break; }
    }
    int parse_argc = (dashdash_pos > 0) ? dashdash_pos : argc;

    long lv;
    int opt;
    optind = 1;
    while ((opt = getopt_long(parse_argc, argv, "p:c:vfh",
                              long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'p':
                if (!opt_int(optarg, cfg.target_pid, 1, INT32_MAX, "pid")) return -1;
                break;
            case 'c': /* loaded separately by find_config_file */ break;
            case 'v': cfg.verbose    = true; break;
            case 'f': cfg.foreground = true; break;
            case 'h': print_usage(argv[0]); std::exit(0);
            case 1001:
                if (!opt_int(optarg, cfg.hot_node,  0, 1023, "hot-node"))  return -1;
                break;
            case 1002:
                if (!opt_int(optarg, cfg.cold_node, 0, 1023, "cold-node")) return -1;
                break;
            case 1003:
                if (!opt_float(optarg, cfg.hot_percentile, 0.0, 100.0, "hot-pct")) return -1;
                break;
            case 1004:
                if (!opt_int(optarg, cfg.frequency, 1, INT32_MAX, "freq")) return -1;
                break;
            case 1005:
                if (!opt_int(optarg, cfg.threads, 1, 1024, "threads")) return -1;
                break;
            case 1006:
                if (!opt_int(optarg, cfg.window_seconds, 1, 86400, "window")) return -1;
                break;
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
                cfg.max_pages_per_window = static_cast<uint64_t>(lv);
                break;
            case 1009: cfg.dry_run = true;       break;
            case 1010: cfg.pidfile  = optarg;     break;
            case 1011: cfg.perf_bin = optarg;     break;
            case 1012:
                if (!opt_int(optarg, cfg.max_idle_windows, 1, INT32_MAX, "max-idle")) return -1;
                break;
            case 1013: cfg.dump_file = optarg;    break;
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

}  // namespace

int cli_load(int argc, char** argv, Config& cfg) {
    // TOML defaults first, CLI flags override.
    std::string config_path = find_config_file(argc, argv);
    if (!config_path.empty()) {
        config_load(cfg, config_path);
    }

    if (parse_args(argc, argv, cfg) != 0) {
        print_usage(argv[0]);
        return -1;
    }

    if (cfg.target_pid <= 0 && cfg.launch_cmd.empty()) {
        log_err("Must specify -p <PID> or -- <command>");
        print_usage(argv[0]);
        return -1;
    }
    return 0;
}
