#include "classifier.h"
#include "util.h"

#include <algorithm>
#include <cmath>
#include <numeric>

void classify_regions(std::vector<Region>& regions,
                      float hot_percentile,
                      int hot_node,
                      int cold_node) {
    if (regions.empty()) return;

    // Collect hotness values from regions that have at least 1 sample
    std::vector<uint64_t> hotness_vals;
    hotness_vals.reserve(regions.size());
    for (const auto& r : regions) {
        if (r.hotness > 0) {
            hotness_vals.push_back(r.hotness);
        }
    }

    if (hotness_vals.empty()) {
        // No samples at all — keep everything where it is
        for (auto& r : regions) {
            r.target_node = r.current_node;
        }
        return;
    }

    // Sort to compute percentile
    std::sort(hotness_vals.begin(), hotness_vals.end());

    // Compute the threshold at hot_percentile
    // hot_percentile=25 means top 75% is hot (>= 25th percentile value)
    float idx_f = (hot_percentile / 100.0f) * (float)(hotness_vals.size() - 1);
    size_t idx = (size_t)std::floor(idx_f);
    if (idx >= hotness_vals.size()) idx = hotness_vals.size() - 1;

    uint64_t threshold = hotness_vals[idx];

    log_verbose("Classification: %zu regions with samples, threshold=%lu "
                "(percentile=%.1f, idx=%zu/%zu)",
                hotness_vals.size(), threshold,
                hot_percentile, idx, hotness_vals.size());

    // Classify each region
    // Regions with hotness >= threshold → hot (keep on hot node)
    // Regions with hotness < threshold OR 0 samples → cold (demote)
    size_t hot_count = 0, cold_count = 0;
    for (auto& r : regions) {
        if (r.hotness >= threshold && threshold > 0) {
            r.target_node = hot_node;
            hot_count++;
        } else {
            r.target_node = cold_node;
            cold_count++;
        }
    }

    log_verbose("Classification result: %zu hot, %zu cold", hot_count, cold_count);
}
