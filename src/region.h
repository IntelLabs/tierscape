#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <atomic>

struct Region {
    uint64_t start_addr;
    uint64_t hotness;       // sample count in current window
    int current_node;       // which NUMA node pages are currently on (-1 = unknown)
    int target_node;        // where they should be after classification
};

class RegionManager {
public:
    RegionManager(uint64_t region_size_bytes);

    // Add a sample address. Returns region index.
    size_t add_sample(uint64_t addr);

    // Reset hotness for all regions (start of new window)
    void reset_hotness();

    // Get all regions with at least one sample
    std::vector<Region>& regions() { return m_regions; }
    const std::vector<Region>& regions() const { return m_regions; }

    size_t region_count() const { return m_regions.size(); }
    uint64_t region_size() const { return m_region_size; }

private:
    uint64_t m_region_size;
    std::vector<Region> m_regions;

    // Map from region base address to index in m_regions
    // Using a sorted vector for cache efficiency
    struct AddrIndex {
        uint64_t base_addr;
        size_t index;
    };
    std::vector<AddrIndex> m_index;

    size_t find_or_create(uint64_t base_addr);
};
