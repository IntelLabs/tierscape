#include "region.h"

RegionManager::RegionManager(uint64_t region_size_bytes)
    : m_region_size(region_size_bytes) {
    if (m_region_size == 0) {
        // Defensive: avoid divide-by-zero downstream. Use 2 MiB default.
        const_cast<uint64_t&>(m_region_size) = 2ULL * 1024 * 1024;
    }
}

void RegionManager::add_sample(uint64_t addr) {
    uint64_t base = (addr / m_region_size) * m_region_size;
    std::lock_guard<std::mutex> lk(m_bucket_mu);
    ++m_bucket[base];
}

std::vector<Region> RegionManager::snapshot_and_swap() {
    std::unordered_map<uint64_t, uint64_t> drained;
    {
        std::lock_guard<std::mutex> lk(m_bucket_mu);
        drained.swap(m_bucket);
    }

    // Bump idle counter for every persistent region; reset for sampled.
    for (auto& kv : m_state) {
        kv.second.idle_windows++;
    }

    std::vector<Region> snap;
    snap.reserve(drained.size());

    for (const auto& kv : drained) {
        uint64_t addr = kv.first;
        uint64_t hot  = kv.second;

        auto it = m_state.find(addr);
        if (it == m_state.end()) {
            it = m_state.emplace(addr, RegionState{}).first;
        }
        it->second.idle_windows = 0;

        Region r;
        r.start_addr   = addr;
        r.hotness      = hot;
        r.current_node = it->second.current_node;
        r.target_node  = -1;
        snap.push_back(r);
    }
    return snap;
}

void RegionManager::update_current_node(uint64_t start_addr, int node) {
    auto it = m_state.find(start_addr);
    if (it != m_state.end()) {
        it->second.current_node = node;
    }
}

size_t RegionManager::evict_idle(int max_idle_windows) {
    size_t evicted = 0;
    for (auto it = m_state.begin(); it != m_state.end(); ) {
        if (it->second.idle_windows >= max_idle_windows) {
            it = m_state.erase(it);
            ++evicted;
        } else {
            ++it;
        }
    }
    return evicted;
}

size_t RegionManager::tracked_count() const {
    return m_state.size();
}
