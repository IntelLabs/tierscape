#pragma once

#include "proc_maps.h"
#include "region.h"

#include <cstdint>
#include <sys/types.h>
#include <vector>

struct MigrateStats {
    uint64_t pages_moved      = 0;
    uint64_t regions_moved    = 0;
    uint64_t pages_promoted   = 0;  // moved to hot_node
    uint64_t pages_demoted    = 0;  // moved to cold_node
    uint64_t already_in_place = 0;
    uint64_t errors           = 0;
    uint64_t skipped_no_vma   = 0;
    int      hot_node         = -1;
    int      cold_node        = -1;
};

// Migrate the regions whose target_node != current_node, in
// parallel across num_threads. Each region is intersected with
// `vmas` so we only call move_pages() for pages that lie in a
// writable anonymous VMA.
//
// Stops if either the per-window page cap or the time budget is
// exhausted.
MigrateStats migrate_regions(std::vector<Region>& regions,
                             uint64_t region_size_bytes,
                             pid_t pid,
                             int num_threads,
                             uint64_t max_pages,
                             int time_limit_sec,
                             const std::vector<Vma>& vmas,
                             int hot_node,
                             int cold_node);
