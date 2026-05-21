#include "proc_maps.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

std::vector<Vma> read_proc_maps(pid_t pid) {
    std::vector<Vma> vmas;
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);

    FILE* f = std::fopen(path, "r");
    if (!f) return vmas;

    char line[4096];
    while (std::fgets(line, sizeof(line), f)) {
        // Format: start-end perms offset dev inode    pathname
        uint64_t start = 0, end = 0;
        char perms[8] = {0};
        char rest[4096] = {0};
        // Use %4s for perms (rwxp)
        int n = std::sscanf(line, "%lx-%lx %4s %*s %*s %*s %[^\n]",
                            &start, &end, perms, rest);
        if (n < 3) continue;

        Vma v{};
        v.start = start;
        v.end = end;
        v.readable   = perms[0] == 'r';
        v.writable   = perms[1] == 'w';
        v.executable = perms[2] == 'x';
        v.path = (n >= 4) ? std::string(rest) : std::string();

        // Trim leading whitespace from path
        size_t i = 0;
        while (i < v.path.size() && (v.path[i] == ' ' || v.path[i] == '\t')) i++;
        if (i) v.path.erase(0, i);

        // Anonymous if no path, or path is a [...] pseudo-region (heap, stack, anon).
        // Exclude file-backed mappings (these usually shouldn't be migrated as
        // they may be shared / read-only file pages).
        v.is_anon = v.path.empty() ||
                    v.path == "[heap]" ||
                    v.path == "[stack]" ||
                    v.path.rfind("[anon:", 0) == 0;

        vmas.push_back(v);
    }
    std::fclose(f);

    std::sort(vmas.begin(), vmas.end(),
              [](const Vma& a, const Vma& b) { return a.start < b.start; });
    return vmas;
}

std::vector<std::pair<uint64_t, uint64_t>>
clip_to_migratable_vmas(const std::vector<Vma>& vmas,
                        uint64_t start, uint64_t end) {
    std::vector<std::pair<uint64_t, uint64_t>> out;
    if (start >= end) return out;

    auto it = std::lower_bound(vmas.begin(), vmas.end(), start,
        [](const Vma& v, uint64_t s) { return v.end <= s; });

    for (; it != vmas.end() && it->start < end; ++it) {
        if (!it->is_anon || !it->writable) continue;
        uint64_t s = std::max(start, it->start);
        uint64_t e = std::min(end,   it->end);
        if (e > s) out.emplace_back(s, e);
    }
    return out;
}
