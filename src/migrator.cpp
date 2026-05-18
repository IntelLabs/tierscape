#include "migrator.h"
#include "util.h"

#include <numa.h>
#include <numaif.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <chrono>
#include <atomic>

static constexpr int PAGE_SIZE = 4096;

static std::atomic<uint64_t> g_pages_moved{0};
static std::atomic<uint64_t> g_regions_moved{0};
static std::atomic<uint64_t> g_already_in_place{0};
static std::atomic<uint64_t> g_errors{0};
static std::atomic<bool> g_stop{false};
static std::chrono::steady_clock::time_point g_start_time;
static int g_time_limit_sec;
static int g_max_pages;

static bool time_exceeded() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - g_start_time).count();
    return elapsed >= g_time_limit_sec;
}

static bool should_stop() {
    return g_stop.load(std::memory_order_relaxed) ||
           (int64_t)g_pages_moved.load(std::memory_order_relaxed) >= g_max_pages ||
           time_exceeded();
}

static void migrate_region(Region& region, uint64_t region_size, pid_t pid) {
    if (should_stop()) return;

    int target = region.target_node;
    if (target < 0) return;

    // Skip if already in place
    if (region.current_node == target) {
        g_already_in_place.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    uint64_t start = region.start_addr;
    int num_pages = region_size / PAGE_SIZE;

    // Allocate arrays for move_pages
    void** pages = (void**)malloc(sizeof(void*) * num_pages);
    int* nodes = (int*)malloc(sizeof(int) * num_pages);
    int* status = (int*)malloc(sizeof(int) * num_pages);

    if (!pages || !nodes || !status) {
        free(pages); free(nodes); free(status);
        g_errors.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    for (int i = 0; i < num_pages; i++) {
        pages[i] = (void*)(start + (uint64_t)i * PAGE_SIZE);
        nodes[i] = target;
        status[i] = -1;
    }

    // move_pages syscall
    long ret = move_pages(pid, num_pages, pages, nodes, status, MPOL_MF_MOVE);

    if (ret == 0) {
        // Count actually moved pages (status[i] == target means success)
        int moved = 0;
        for (int i = 0; i < num_pages; i++) {
            if (status[i] == target || status[i] >= 0) {
                moved++;
            }
        }
        if (moved > 0) {
            g_pages_moved.fetch_add(moved, std::memory_order_relaxed);
            g_regions_moved.fetch_add(1, std::memory_order_relaxed);
            region.current_node = target;
        }
    } else {
        // move_pages can return -1 with partial success
        // Check status array for individual page results
        int moved = 0;
        for (int i = 0; i < num_pages; i++) {
            if (status[i] >= 0) moved++;
        }
        if (moved > 0) {
            g_pages_moved.fetch_add(moved, std::memory_order_relaxed);
            g_regions_moved.fetch_add(1, std::memory_order_relaxed);
            region.current_node = target;
        } else {
            g_errors.fetch_add(1, std::memory_order_relaxed);
        }
    }

    free(pages);
    free(nodes);
    free(status);
}

static void worker_thread(std::vector<Region>* regions, uint64_t region_size,
                           pid_t pid, size_t start_idx, size_t end_idx) {
    for (size_t i = start_idx; i < end_idx; i++) {
        if (should_stop()) break;
        Region& r = (*regions)[i];
        if (r.target_node >= 0 && r.target_node != r.current_node) {
            migrate_region(r, region_size, pid);
        }
    }
}

MigrateStats migrate_regions(std::vector<Region>& regions,
                             uint64_t region_size_bytes,
                             pid_t pid,
                             int num_threads,
                             int max_pages,
                             int time_limit_sec) {
    // Reset globals
    g_pages_moved = 0;
    g_regions_moved = 0;
    g_already_in_place = 0;
    g_errors = 0;
    g_stop = false;
    g_max_pages = max_pages;
    g_time_limit_sec = time_limit_sec;
    g_start_time = std::chrono::steady_clock::now();

    size_t n = regions.size();
    if (n == 0 || num_threads <= 0) {
        return MigrateStats{};
    }

    // Divide work among threads
    size_t chunk = n / num_threads;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        size_t start_idx = t * chunk;
        size_t end_idx = (t == num_threads - 1) ? n : (t + 1) * chunk;
        threads.emplace_back(worker_thread, &regions, region_size_bytes,
                             pid, start_idx, end_idx);
    }

    for (auto& th : threads) {
        th.join();
    }

    MigrateStats stats;
    stats.pages_moved = g_pages_moved.load();
    stats.regions_moved = g_regions_moved.load();
    stats.already_in_place = g_already_in_place.load();
    stats.errors = g_errors.load();

    log_verbose("Migration: %lu pages moved, %lu regions moved, "
                "%lu already in place, %lu errors",
                stats.pages_moved, stats.regions_moved,
                stats.already_in_place, stats.errors);

    return stats;
}
