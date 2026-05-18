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

    // Clip the region to a migratable VMA. Skip if no overlap.
    uint64_t r_start = region.start_addr;
    uint64_t r_end   = r_start + ctx.region_size;
    uint64_t c_start = 0, c_end = 0;
    if (!clip_to_migratable_vma(*ctx.vmas, r_start, r_end, c_start, c_end)) {
        ctx.skipped_no_vma.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const uint64_t pgsz = ctx.page_size;
    const size_t num_pages = static_cast<size_t>((c_end - c_start) / pgsz);
    if (num_pages == 0) return;

    std::vector<void*> pages(num_pages);
    std::vector<int>   nodes(num_pages, target);
    std::vector<int>   status(num_pages, -1);

    for (size_t i = 0; i < num_pages; ++i) {
        pages[i] = reinterpret_cast<void*>(c_start + i * pgsz);
    }

    long ret = move_pages(ctx.pid, num_pages,
                          pages.data(), nodes.data(),
                          status.data(), MPOL_MF_MOVE);
    (void)ret;  // partial-success is communicated via status[]

    size_t moved = 0;
    for (size_t i = 0; i < num_pages; ++i) {
        if (status[i] >= 0) ++moved;
    }
    if (moved > 0) {
        ctx.pages_moved.fetch_add(moved, std::memory_order_relaxed);
        ctx.regions_moved.fetch_add(1, std::memory_order_relaxed);
        if (target == ctx.hot_node) {
            ctx.pages_promoted.fetch_add(moved, std::memory_order_relaxed);
        } else {
            ctx.pages_demoted.fetch_add(moved, std::memory_order_relaxed);
        }
        region.current_node = target;
    } else if (ret < 0) {
        ctx.errors.fetch_add(1, std::memory_order_relaxed);
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
