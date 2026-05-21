#include "classifier.h"
#include "util.h"

#include <algorithm>
#include <cmath>

void classify_regions(std::vector<Region>& regions,
                      float hot_percentile,
                      int hot_node,
                      int cold_node) {
    if (regions.empty()) return;

    // Build the hotness vector over ALL tracked regions (including silent
    // ones with hotness=0). This sizes the hot-tier "budget" against the
    // full footprint, so the top (100-pct)% of the address space lands hot
    // — not just the top fraction of whatever happened to be sampled this
    // window. Silent regions are always demotion candidates because their
    // hotness is below any positive threshold.
    std::vector<uint64_t> vals;
    vals.reserve(regions.size());
    size_t nonzero = 0;
    for (const auto& r : regions) {
        vals.push_back(r.hotness);
        if (r.hotness > 0) ++nonzero;
    }

    if (nonzero == 0) {
        // No accesses observed anywhere → leave placement unchanged.
        for (auto& r : regions) r.target_node = r.current_node;
        return;
    }

    // Compute the percentile-indexed element via nth_element (avg O(n))
    // rather than sorting the whole vector (O(n log n)).
    float pct = std::max(0.0f, std::min(100.0f, hot_percentile));
    float idx_f = (pct / 100.0f) * static_cast<float>(vals.size() - 1);
    size_t idx = static_cast<size_t>(std::floor(idx_f));
    if (idx >= vals.size()) idx = vals.size() - 1;

    std::nth_element(vals.begin(), vals.begin() + idx, vals.end());
    // Floor threshold at 1 so a percentile that lands on a zero (active
    // set smaller than the hot budget) still promotes every accessed region.
    const uint64_t threshold = std::max<uint64_t>(vals[idx], 1);

    log_verbose("Classify: %zu of %zu regions had samples, threshold=%lu "
                "(percentile=%.1f, idx=%zu/%zu, over full tracked set)",
                nonzero, regions.size(), threshold, pct, idx, vals.size());

    size_t hot_count = 0, cold_count = 0;
    for (auto& r : regions) {
        if (r.hotness >= threshold) {
            r.target_node = hot_node;
            ++hot_count;
        } else {
            r.target_node = cold_node;
            ++cold_count;
        }
    }
    log_verbose("Classify result: %zu hot, %zu cold (of %zu tracked)",
                hot_count, cold_count, regions.size());
}
