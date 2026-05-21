#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

// Per-window snapshot of a region. The classifier and migrator
// operate on instances of this struct.
struct Region {
    uint64_t start_addr  = 0;
    uint64_t hotness     = 0;   // sample count in current window
    int      current_node = -1; // last-known NUMA node (-1 = unknown)
    int      target_node  = -1; // classifier output for this window
};

// Persistent per-region metadata kept across windows.
struct RegionState {
    int current_node = -1;
    int idle_windows = 0;       // consecutive windows with zero hotness
};

// Thread-safe region accounting.
//
//   Sampler thread:  add_sample(addr)               (called ~millions/s)
//   Window thread:   snapshot_and_swap()            (once per window)
//                    update_current_node(addr, n)   (post-migration)
//                    evict_idle(max_idle_windows)   (end of window)
//
// The bucket holds samples for the current window. snapshot_and_swap
// drains it under a brief lock, merges into persistent state, and
// returns a copy of regions with hotness > 0 this window.
class RegionManager {
public:
    explicit RegionManager(uint64_t region_size_bytes);

    void add_sample(uint64_t addr);

    std::vector<Region> snapshot_and_swap();

    void update_current_node(uint64_t start_addr, int node);

    size_t evict_idle(int max_idle_windows);

    uint64_t region_size() const { return m_region_size; }
    size_t   tracked_count() const;

private:
    // Not const: validated once in the ctor (zero -> 2 MiB default).
    uint64_t m_region_size;

    mutable std::mutex m_bucket_mu;
    std::unordered_map<uint64_t, uint64_t> m_bucket;  // addr -> hotness

    // Persistent state. Only the window thread touches this.
    std::unordered_map<uint64_t, RegionState> m_state;
};
