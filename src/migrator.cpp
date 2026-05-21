#include "migrator.h"
#include "util.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <numa.h>
#include <numaif.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

struct MigCtx {
    pid_t pid;
    uint64_t region_size;
    uint64_t page_size;
    const std::vector<Vma>* vmas;
    int      time_limit_sec;
    uint64_t max_pages;

    std::chrono::steady_clock::time_point start;

    int hot_node;
    int cold_node;

    std::atomic<uint64_t> pages_moved{0};
    std::atomic<uint64_t> regions_moved{0};
    std::atomic<uint64_t> pages_promoted{0};
    std::atomic<uint64_t> pages_demoted{0};
    std::atomic<uint64_t> already_in_place{0};
    std::atomic<uint64_t> errors{0};
    std::atomic<uint64_t> skipped_no_vma{0};
    std::atomic<bool>     stop{false};
};

bool budget_exceeded(const MigCtx& c) {
    if (c.stop.load(std::memory_order_relaxed)) return true;
    if (c.pages_moved.load(std::memory_order_relaxed) >= c.max_pages) return true;
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - c.start).count();
    return elapsed >= c.time_limit_sec;
}

void migrate_one(Region& region, MigCtx& ctx) {
    if (budget_exceeded(ctx)) return;

    int target = region.target_node;
    if (target < 0) return;
    if (region.current_node == target) {
        ctx.already_in_place.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const uint64_t pgsz = ctx.page_size;
    const uint64_t r_start = region.start_addr;
    const uint64_t r_end   = r_start + ctx.region_size;

    // A region may straddle multiple VMAs; migrate each migratable
    // sub-range independently.
    auto ranges = clip_to_migratable_vmas(*ctx.vmas, r_start, r_end);
    if (ranges.empty()) {
        ctx.skipped_no_vma.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // Process pages in fixed-size chunks so we (a) bound peak memory
    // per call and (b) re-check the per-window budget frequently.
    constexpr size_t kChunkPages = 1024;
    uint64_t region_pages_moved = 0;

    for (const auto& seg : ranges) {
        if (budget_exceeded(ctx)) break;

        for (uint64_t off = seg.first; off < seg.second; off += kChunkPages * pgsz) {
            if (budget_exceeded(ctx)) break;

            uint64_t chunk_end = std::min<uint64_t>(off + kChunkPages * pgsz, seg.second);
            size_t   n         = static_cast<size_t>((chunk_end - off) / pgsz);
            if (n == 0) continue;

            // Respect the global page cap.
            uint64_t already = ctx.pages_moved.load(std::memory_order_relaxed);
            if (already >= ctx.max_pages) break;
            uint64_t remaining = ctx.max_pages - already;
            if (n > remaining) n = static_cast<size_t>(remaining);
            if (n == 0) break;

            std::vector<void*> pages(n);
            std::vector<int>   nodes(n, target);
            std::vector<int>   status(n, -1);
            for (size_t i = 0; i < n; ++i) {
                pages[i] = reinterpret_cast<void*>(off + i * pgsz);
            }

            long ret = move_pages(ctx.pid, n,
                                  pages.data(), nodes.data(),
                                  status.data(), MPOL_MF_MOVE);

            size_t moved = 0;
            for (size_t i = 0; i < n; ++i) {
                if (status[i] >= 0) ++moved;
            }

            if (moved > 0) {
                ctx.pages_moved.fetch_add(moved, std::memory_order_relaxed);
                if (target == ctx.hot_node)
                    ctx.pages_promoted.fetch_add(moved, std::memory_order_relaxed);
                else
                    ctx.pages_demoted.fetch_add(moved, std::memory_order_relaxed);
                region_pages_moved += moved;
            }

            // The whole-call return value is negative on outright syscall
            // failure; non-negative means partial-success (per-page status
            // is authoritative). Only count syscall failures as errors.
            if (ret < 0) {
                ctx.errors.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    if (region_pages_moved > 0) {
        ctx.regions_moved.fetch_add(1, std::memory_order_relaxed);
        region.current_node = target;
    }
}

void worker(std::vector<Region>* regions, MigCtx* ctx,
            size_t start_idx, size_t end_idx) {
    for (size_t i = start_idx; i < end_idx; ++i) {
        if (budget_exceeded(*ctx)) break;
        Region& r = (*regions)[i];
        if (r.target_node >= 0 && r.target_node != r.current_node) {
            migrate_one(r, *ctx);
        }
    }
}

}  // namespace

MigrateStats migrate_regions(std::vector<Region>& regions,
                             uint64_t region_size_bytes,
                             pid_t pid,
                             int num_threads,
                             uint64_t max_pages,
                             int time_limit_sec,
                             const std::vector<Vma>& vmas,
                             int hot_node,
                             int cold_node) {
    MigrateStats out;
    if (regions.empty() || num_threads <= 0) return out;

    MigCtx ctx;
    ctx.pid             = pid;
    ctx.region_size     = region_size_bytes;
    ctx.page_size       = static_cast<uint64_t>(sysconf(_SC_PAGESIZE));
    ctx.vmas            = &vmas;
    ctx.time_limit_sec  = time_limit_sec;
    ctx.max_pages       = max_pages;
    ctx.hot_node        = hot_node;
    ctx.cold_node       = cold_node;
    ctx.start           = std::chrono::steady_clock::now();

    const size_t n = regions.size();
    const size_t chunk = (n + num_threads - 1) / num_threads;

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (int t = 0; t < num_threads; ++t) {
        size_t s = t * chunk;
        size_t e = std::min(n, s + chunk);
        if (s >= e) break;
        threads.emplace_back(worker, &regions, &ctx, s, e);
    }
    for (auto& th : threads) th.join();

    out.pages_moved      = ctx.pages_moved.load();
    out.regions_moved    = ctx.regions_moved.load();
    out.pages_promoted   = ctx.pages_promoted.load();
    out.pages_demoted    = ctx.pages_demoted.load();
    out.already_in_place = ctx.already_in_place.load();
    out.errors           = ctx.errors.load();
    out.skipped_no_vma   = ctx.skipped_no_vma.load();
    out.hot_node         = hot_node;
    out.cold_node        = cold_node;

    log_verbose("Migration: %lu pages (%lu promoted, %lu demoted), "
                "%lu regions moved, %lu in-place, "
                "%lu errors, %lu skipped (no anon VMA)",
                out.pages_moved, out.pages_promoted, out.pages_demoted,
                out.regions_moved, out.already_in_place,
                out.errors, out.skipped_no_vma);
    return out;
}
