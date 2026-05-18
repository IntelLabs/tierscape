#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct Config {
    // [tiers]
    int hot_node = 0;
    int cold_node = 1;

    // [sampling]
    std::vector<std::string> events = {
        "cpu/event=0xd0,umask=0x81/ppu",
        "cpu/event=0xd0,umask=0x82/ppu",
        "cpu/event=0xd0,umask=0x11/ppu",
        "cpu/event=0xd0,umask=0x12/ppu",
    };
    int frequency = 10000;      // perf -c period (sample every N events)
    int window_seconds = 20;

    // [classification]
    float hot_percentile = 25.0f;

    // [migration]
    int threads = 2;
    int max_pages_per_window = 5000000;
    std::string region_size_str = "2M";
    uint64_t region_size_bytes = 2 * 1024 * 1024;

    // [daemon]
    std::string pidfile = "/tmp/tierscaped.pid";
    bool verbose = false;
    std::string log_file;
    std::string perf_bin = "/usr/bin/perf";

    // runtime (not from config file)
    pid_t target_pid = -1;
    bool foreground = false;
    bool dry_run = false;
    std::vector<std::string> launch_cmd;
};

// Load config from TOML file. Returns 0 on success, -1 on error.
int config_load(Config& cfg, const std::string& path);

// Parse region size string like "2M", "4K", "1G" into bytes.
uint64_t parse_size(const std::string& s);
