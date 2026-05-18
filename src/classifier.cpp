#include "classifier.h"
#include "util.h"

#include <algorithm>
#include <cmath>

void classify_regions(std::vector<Region>& regions,
                      float hot_percentile,
                      int hot_node,
                      int cold_node) {
    if (regions.empty()) return;

    std::vector<uint64_t> hot_vals;
    hot_vals.reserve(regions.size());
    for (const auto& r : regions) {
        if (r.hotness > 0) hot_vals.push_back(r.hotness);
    }

    if (hot_vals.empty()) {
        for (auto& r : regions) r.target_node = r.current_node;
        return;
    }

    std::sort(hot_vals.begin(), hot_vals.end());

    // Clamp percentile and compute threshold index.
    float pct = std::max(0.0f, std::min(100.0f, hot_percentile));
    float idx_f = (pct / 100.0f) * static_cast<float>(hot_vals.size() - 1);
    size_t idx = static_cast<size_t>(std::floor(idx_f));
    if (idx >= hot_vals.size()) idx = hot_vals.size() - 1;

    const uint64_t threshold = hot_vals[idx];

    log_verbose("Classify: %zu sampled regions, threshold=%lu "
                "(percentile=%.1f, idx=%zu/%zu)",
                hot_vals.size(), threshold, pct, idx, hot_vals.size());

    size_t hot_count = 0, cold_count = 0;
    for (auto& r : regions) {
        if (r.hotness >= threshold && threshold > 0) {
            r.target_node = hot_node;
            ++hot_count;
        } else {
            r.target_node = cold_node;
            ++cold_count;
        }
    }
    log_verbose("Classify result: %zu hot, %zu cold", hot_count, cold_count);
}
