#include "sanity.h"
#include "util.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <numa.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace {

int check_numa_node(int node, const char* label) {
    char path[128];
    std::snprintf(path, sizeof(path),
                  "/sys/devices/system/node/node%d", node);

    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        log_err("NUMA %s node %d does not exist (%s)", label, node, path);
        return -1;
    }

    long long free_bytes = 0;
    long long size = numa_node_size64(node, &free_bytes);
    if (size <= 0) {
        log_err("NUMA %s node %d has zero memory (size64=%lld)",
                label, node, size);
        return -1;
    }

    log_verbose("NUMA %s node %d: %lld MB total, %lld MB free",
                label, node, size / (1024 * 1024),
                free_bytes / (1024 * 1024));
    return 0;
}

int check_perf(const Config& cfg) {
    if (access(cfg.perf_bin.c_str(), X_OK) != 0) {
        log_err("perf binary not found or not executable: %s",
                cfg.perf_bin.c_str());
        return -1;
    }

    std::ifstream paranoid("/proc/sys/kernel/perf_event_paranoid");
    if (paranoid.is_open()) {
        int level = 4;
        paranoid >> level;
        if (level > 1 && getuid() != 0) {
            log_err("perf_event_paranoid=%d and not running as root. "
                    "Set it to <= 1 or run as root.", level);
            return -1;
        }
        log_verbose("perf_event_paranoid=%d (uid=%d)", level, getuid());
    }
    return 0;
}

// Validate all PMU events in a single `perf stat` call.
int check_pmu_events(const Config& cfg) {
    if (cfg.events.empty()) {
        log_err("No PMU events configured (or all rejected as unsafe)");
        return -1;
    }

    // Events are already whitelisted in config_load (is_valid_event_name),
    // so it is safe to interpolate them into a shell command.
    std::string events_arg;
    for (size_t i = 0; i < cfg.events.size(); ++i) {
        if (i) events_arg += ",";
        events_arg += cfg.events[i];
    }

    char cmd[2048];
    std::snprintf(cmd, sizeof(cmd),
                  "%s stat -e %s -a -- sleep 0.01 2>&1",
                  cfg.perf_bin.c_str(), events_arg.c_str());

    FILE* fp = popen(cmd, "r");
    if (!fp) {
        log_err("Failed to invoke perf stat for event validation");
        return -1;
    }

    char buf[1024];
    bool fail = false;
    std::string output;
    while (std::fgets(buf, sizeof(buf), fp)) {
        output += buf;
        if (std::strstr(buf, "not supported") ||
            std::strstr(buf, "No such file") ||
            std::strstr(buf, "invalid or unsupported")) {
            fail = true;
        }
    }
    int ret = pclose(fp);

    if (fail || ret != 0) {
        log_err("PMU event validation failed (rc=%d). perf output:\n%s",
                ret, output.c_str());
        return -1;
    }
    log_verbose("All %zu PMU events validated", cfg.events.size());
    return 0;
}

int check_process(const Config& cfg) {
    if (cfg.target_pid <= 0) return 0;  // launch mode

    if (!is_process_running(cfg.target_pid)) {
        log_err("Target process %d does not exist", cfg.target_pid);
        return -1;
    }

    char path[64];
    std::snprintf(path, sizeof(path),
                  "/proc/%d/pagemap", cfg.target_pid);
    if (access(path, R_OK) != 0) {
        log_err("Cannot read %s (need root or CAP_SYS_PTRACE)", path);
        return -1;
    }

    log_verbose("Target process %d is accessible", cfg.target_pid);
    return 0;
}

}  // namespace

int sanity_check_all(const Config& cfg) {
    log_info("Running sanity checks...");

    if (numa_available() < 0) {
        log_err("NUMA is not available on this system");
        return -1;
    }

    if (cfg.hot_node == cfg.cold_node) {
        log_err("hot_node (%d) and cold_node (%d) must be different",
                cfg.hot_node, cfg.cold_node);
        return -1;
    }

    if (check_numa_node(cfg.hot_node,  "hot")  != 0) return -1;
    if (check_numa_node(cfg.cold_node, "cold") != 0) return -1;
    if (check_perf(cfg)        != 0) return -1;
    if (check_pmu_events(cfg)  != 0) return -1;
    if (check_process(cfg)     != 0) return -1;

    log_info("All sanity checks passed.");
    return 0;
}
