#pragma once

#include <cstdint>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>

// A single virtual memory area (VMA) from /proc/<pid>/maps.
struct Vma {
    uint64_t start;      // inclusive
    uint64_t end;        // exclusive
    bool readable;
    bool writable;
    bool executable;
    bool is_anon;        // true if pathname is empty or [heap]/[stack]/...
    std::string path;    // pathname column (may be empty or "[heap]" etc.)
};

// Read /proc/<pid>/maps. Returns empty vector on error.
// Sorted by start address.
std::vector<Vma> read_proc_maps(pid_t pid);

// Return all migratable sub-ranges of [start, end) that lie inside an
// anonymous, writable VMA. Each returned pair is [seg_start, seg_end).
// Empty result means no overlap.
std::vector<std::pair<uint64_t, uint64_t>>
clip_to_migratable_vmas(const std::vector<Vma>& vmas,
                        uint64_t start, uint64_t end);
