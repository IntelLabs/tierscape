#include "region.h"

static constexpr uint64_t k_default_region_size = 2ULL * 1024 * 1024;  // 2 MiB

RegionManager::RegionManager(uint64_t region_size_bytes)
    : m_region_size(region_size_bytes ? region_size_bytes : k_default_region_size) {}

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

    // Bump idle counter for every persistent region; sampled regions
    // get their counter reset below.
    for (auto& kv : m_state) {
        kv.second.idle_windows++;
    }

    // Ensure every region sampled this window has persistent state and
    // mark it as fresh.
    for (const auto& kv : drained) {
        auto it = m_state.find(kv.first);
        if (it == m_state.end()) {
            it = m_state.emplace(kv.first, RegionState{}).first;
        }
        it->second.idle_windows = 0;
    }

    // Emit one Region per *tracked* entry. Sampled regions carry their
    // real hotness; silent-but-tracked regions get hotness=0 so the
    // classifier can still evaluate them (and demote them to cold) this
    // window.
    std::vector<Region> snap;
    snap.reserve(m_state.size());
    for (const auto& kv : m_state) {
        Region r;
        r.start_addr   = kv.first;
        auto dit = drained.find(kv.first);
        r.hotness      = (dit != drained.end()) ? dit->second : 0;
        r.current_node = kv.second.current_node;
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
