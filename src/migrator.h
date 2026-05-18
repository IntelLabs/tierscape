#pragma once

#include "region.h"
#include <cstdint>
#include <vector>
#include <atomic>
#include <sys/types.h>

struct MigrateStats {
    uint64_t pages_moved = 0;
    uint64_t regions_moved = 0;
    uint64_t already_in_place = 0;
    uint64_t errors = 0;
};

// Migrate regions to their target_node using move_pages().
// Uses num_threads parallel workers.
// Stops if time_limit_sec exceeded or max_pages reached.
// Returns migration statistics.
MigrateStats migrate_regions(std::vector<Region>& regions,
                             uint64_t region_size_bytes,
                             pid_t pid,
                             int num_threads,
                             int max_pages,
                             int time_limit_sec);
