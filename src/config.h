#pragma once

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <vector>

struct Config {
    // [tiers]
    int hot_node  = 0;
    int cold_node = 1;

    // [sampling]
    std::vector<std::string> events = {
        "mem_inst_retired.all_loads:P",
        "mem_inst_retired.all_stores:P",
    };
    int frequency      = 10000;     // perf -c period
    int window_seconds = 20;

    // [classification]
    // Percentile (0..100) of per-region hotness. Regions whose hotness
    // is *below* this percentile are demoted to the cold tier; regions
    // at or above stay on the hot tier.
    //
    //   hot_percentile = 25  => demote bottom 25% (75% stays hot)
    //   hot_percentile = 75  => demote bottom 75% (only top 25% hot)
    //
    // Implementation note: the threshold is floored at 1 in
    // classify_regions(), so any region with at least one sample is
    // promoted when the hot budget exceeds the active set size.
    float hot_percentile = 25.0f;

    // [migration]
    int      threads              = 2;
    uint64_t max_pages_per_window = 5'000'000;
    std::string region_size_str   = "2M";
    uint64_t    region_size_bytes = 2ULL * 1024 * 1024;
    int      max_idle_windows     = 10;  // evict regions idle for this long

    // [daemon]
    std::string pidfile  = "/tmp/tierscaped.pid";
    bool        verbose  = false;
    std::string log_file;
    std::string perf_bin = "/usr/bin/perf";
    std::string dump_file;  // If non-empty, dump "time_ms addr" per sample

    // Runtime (not from config file)
    pid_t                    target_pid = -1;
    bool                     foreground = false;
    bool                     dry_run    = false;
    std::vector<std::string> launch_cmd;
};

// Load config from TOML file. Returns 0 on success, -1 on error.
// Unknown keys are warned but ignored.
int config_load(Config& cfg, const std::string& path);

// Parse "2M", "4K", "1G", "4KiB", or plain bytes into a byte count.
// Returns 0 on parse error.
uint64_t parse_size(const std::string& s);

// Strict whitelist check for a perf event name. Returns true if `s`
// contains only characters that are safe to interpolate into a perf
// command line (alnum and "_.:/-@=,"). Used to prevent shell injection.
bool is_valid_event_name(const std::string& s);
