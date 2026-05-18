#pragma once

#include <cstdint>
#include <string>
#include <sys/types.h>
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

// Return true if [start, end) is fully or partially inside any
// migratable VMA (anonymous + writable). Out-params clip the range to
// the first overlapping VMA's bounds.
bool clip_to_migratable_vma(const std::vector<Vma>& vmas,
                            uint64_t start, uint64_t end,
                            uint64_t& out_start, uint64_t& out_end);
