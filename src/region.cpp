#include "region.h"
#include <algorithm>
#include <unordered_map>

RegionManager::RegionManager(uint64_t region_size_bytes)
    : m_region_size(region_size_bytes) {}

size_t RegionManager::add_sample(uint64_t addr) {
    uint64_t base = (addr / m_region_size) * m_region_size;
    size_t idx = find_or_create(base);
    m_regions[idx].hotness++;
    return idx;
}

void RegionManager::reset_hotness() {
    for (auto& r : m_regions) {
        r.hotness = 0;
    }
}

size_t RegionManager::find_or_create(uint64_t base_addr) {
    // Binary search in sorted index
    auto it = std::lower_bound(m_index.begin(), m_index.end(), base_addr,
        [](const AddrIndex& ai, uint64_t addr) { return ai.base_addr < addr; });

    if (it != m_index.end() && it->base_addr == base_addr) {
        return it->index;
    }

    // Create new region
    size_t new_idx = m_regions.size();
    Region r;
    r.start_addr = base_addr;
    r.hotness = 0;
    r.current_node = -1;
    r.target_node = -1;
    m_regions.push_back(r);

    // Insert into sorted index
    AddrIndex ai{base_addr, new_idx};
    m_index.insert(it, ai);

    return new_idx;
}
