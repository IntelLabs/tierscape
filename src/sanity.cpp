#include "sanity.h"
#include "util.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <numa.h>

// Check that a NUMA node exists and has non-zero memory
static int check_numa_node(int node, const char* label) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/devices/system/node/node%d", node);

    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        log_err("NUMA %s node %d does not exist (no %s)", label, node, path);
        return -1;
    }

    // Check MemTotal from meminfo
    char meminfo_path[160];
    snprintf(meminfo_path, sizeof(meminfo_path),
             "/sys/devices/system/node/node%d/meminfo", node);

    std::ifstream meminfo(meminfo_path);
    if (!meminfo.is_open()) {
        log_err("Cannot open %s", meminfo_path);
        return -1;
    }

    std::string line;
    uint64_t mem_total_kb = 0;
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal") != std::string::npos) {
            // Format: "Node X MemTotal:    12345 kB"
            const char* ptr = strstr(line.c_str(), "MemTotal:");
            if (ptr) {
                ptr += strlen("MemTotal:");
                mem_total_kb = strtoull(ptr, nullptr, 10);
            }
            break;
        }
    }

    if (mem_total_kb == 0) {
        log_err("NUMA %s node %d has 0 memory", label, node);
        return -1;
    }

    // Also verify via libnuma
    long long free_bytes = 0;
    long long size = numa_node_size64(node, &free_bytes);
    if (size <= 0) {
        log_err("NUMA %s node %d: numa_node_size64 reports %lld bytes", label, node, size);
        return -1;
    }

    log_verbose("NUMA %s node %d: %llu MB total, %lld MB free",
                label, node, mem_total_kb / 1024, free_bytes / (1024 * 1024));
    return 0;
}

// Check perf binary exists and is executable
static int check_perf(const Config& cfg) {
    if (access(cfg.perf_bin.c_str(), X_OK) != 0) {
        log_err("perf binary not found or not executable: %s", cfg.perf_bin.c_str());
        return -1;
    }

    // Check perf_event_paranoid
    std::ifstream paranoid("/proc/sys/kernel/perf_event_paranoid");
    if (paranoid.is_open()) {
        int level = 4;
        paranoid >> level;
        if (level > 1 && getuid() != 0) {
            log_err("perf_event_paranoid=%d and not running as root. "
                    "Set /proc/sys/kernel/perf_event_paranoid to 1 or run as root.", level);
            return -1;
        }
        log_verbose("perf_event_paranoid=%d (uid=%d)", level, getuid());
    }

    return 0;
}

// Check that PMU counters are available by doing a quick perf stat
static int check_pmu_events(const Config& cfg) {
    for (const auto& event : cfg.events) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "%s stat -e %s -a -- sleep 0.01 2>&1 | grep -v 'not supported' | grep -q '%s'",
                 cfg.perf_bin.c_str(), event.c_str(), event.c_str());

        // Simpler check: just run perf stat and look for "not supported" in output
        char cmd2[512];
        snprintf(cmd2, sizeof(cmd2),
                 "%s stat -e %s -a -- sleep 0.01 2>&1",
                 cfg.perf_bin.c_str(), event.c_str());

        FILE* fp = popen(cmd2, "r");
        if (!fp) {
            log_err("Failed to run perf stat for event: %s", event.c_str());
            return -1;
        }

        char buf[1024];
        bool not_supported = false;
        while (fgets(buf, sizeof(buf), fp)) {
            if (strstr(buf, "not supported") || strstr(buf, "not counted") ||
                strstr(buf, "No such file") || strstr(buf, "invalid")) {
                not_supported = true;
            }
        }
        int ret = pclose(fp);

        if (not_supported || ret != 0) {
            log_err("PMU event not supported: %s", event.c_str());
            return -1;
        }
    }
    log_verbose("All %zu PMU events validated", cfg.events.size());
    return 0;
}

// Check target process (if PID provided)
static int check_process(const Config& cfg) {
    if (cfg.target_pid <= 0) return 0;  // will launch, no PID yet

    if (!is_process_running(cfg.target_pid)) {
        log_err("Target process %d does not exist", cfg.target_pid);
        return -1;
    }

    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/pagemap", cfg.target_pid);
    if (access(path, R_OK) != 0) {
        log_err("Cannot read %s (need root or CAP_SYS_PTRACE)", path);
        return -1;
    }

    log_verbose("Target process %d is accessible", cfg.target_pid);
    return 0;
}

int sanity_check_all(const Config& cfg) {
    log_info("Running sanity checks...");

    // Initialize libnuma
    if (numa_available() < 0) {
        log_err("NUMA is not available on this system");
        return -1;
    }

    // Check nodes
    if (cfg.hot_node == cfg.cold_node) {
        log_err("hot_node (%d) and cold_node (%d) must be different", cfg.hot_node, cfg.cold_node);
        return -1;
    }

    if (check_numa_node(cfg.hot_node, "hot") != 0) return -1;
    if (check_numa_node(cfg.cold_node, "cold") != 0) return -1;

    // Check perf
    if (check_perf(cfg) != 0) return -1;

    // Check PMU events
    if (check_pmu_events(cfg) != 0) return -1;

    // Check target process
    if (check_process(cfg) != 0) return -1;

    log_info("All sanity checks passed.");
    return 0;
}
