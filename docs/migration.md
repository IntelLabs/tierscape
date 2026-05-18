# Migration

The migrator is the only component that calls into the kernel to
move pages. It runs once per window, in parallel across
`cfg.threads` worker threads.

## Public API

```cpp
MigrateStats migrate_regions(std::vector<Region>& regions,
                             uint64_t region_size_bytes,
                             pid_t pid,
                             int num_threads,
                             uint64_t max_pages,
                             int time_limit_sec,
                             const std::vector<Vma>& vmas);
```

Returns:

```cpp
struct MigrateStats {
    uint64_t pages_moved;
    uint64_t regions_moved;
    uint64_t already_in_place;
    uint64_t errors;
    uint64_t skipped_no_vma;
};
```

## Per-region flow

For each `Region` where `target_node != current_node`:

1. **Budget check** — if `pages_moved >= max_pages` or
   elapsed `>= time_limit_sec`, stop.
2. **VMA clip** — `clip_to_migratable_vma(vmas, r_start, r_end,
   out_start, out_end)`:
   * Binary-search `vmas` for the first VMA whose `end > r_start`.
   * If no overlap, or the VMA isn't anonymous + writable, count in
     `skipped_no_vma` and skip.
   * Else clip `[r_start, r_end)` to that VMA's bounds. (Regions
     spanning multiple VMAs only migrate the first segment for now.)
3. **Build arrays** for `move_pages`:
   ```cpp
   std::vector<void*> pages(N);
   std::vector<int>   nodes(N, target_node);
   std::vector<int>   status(N, -1);
   for (i) pages[i] = (char*)c_start + i * page_size;
   ```
   Page size from `sysconf(_SC_PAGESIZE)` — not hard-coded 4096.
4. **Invoke** `move_pages(pid, N, pages, nodes, status, MPOL_MF_MOVE)`.
5. **Account** — pages with `status[i] >= 0` count as moved. The
   region's `current_node` is updated to `target_node` if any page
   succeeded; otherwise the region is counted under `errors`.

## Budgets

Two independent caps protect the workload from migration stalls:

* **Page cap** — `max_pages_per_window` (default 5 000 000 ≈ 20 GiB).
* **Time cap** — 90 % of `window_seconds`. The remaining 10 % is
  reserved for snapshot, classification, and `/proc/maps` parsing.

Both are checked at every region boundary by every worker. If
either trips, in-flight `move_pages` calls finish but no new regions
are started.

## Why VMA filtering?

A 2 MiB region contains 512 × 4 KiB pages. In a sparse
workload (e.g., a malloc heap with holes), only a fraction of those
pages are mapped. The kernel returns `-ENOENT` in `status[]` for
unmapped pages — silent, but wasteful: each call still copies the
512-entry array across the syscall boundary.

By clipping to the actual VMA bounds, we:

* Issue smaller `move_pages` calls for partial regions.
* Skip regions with **zero** overlap (e.g., samples that hit a
  file-backed library mapping).
* Avoid migrating non-anonymous pages that could be shared
  read-only or backed by inode caches.

The `is_anon` check accepts:

* Empty pathname (anon mmap)
* `[heap]`
* `[stack]`
* `[anon:*]` (named anon, since kernel 5.17)

It excludes:

* `/usr/lib/...` (file-backed code/data)
* `[vdso]`, `[vvar]`, `[vsyscall]` (kernel-managed)
* SHM segments

## Concurrency

* `MigCtx` is a stack-allocated struct passed by pointer to each
  worker thread.
* Counters are `std::atomic<uint64_t>` with `memory_order_relaxed` —
  exact ordering doesn't matter, only the final aggregate.
* Regions are statically partitioned into N contiguous chunks
  (no work-stealing). For uniform region sizes this is fine; a
  highly skewed workload could benefit from dynamic scheduling.

## Failure modes

| Symptom | Cause | Diagnostic |
|---------|-------|-----------|
| All windows show `errors=N, pages_moved=0` | Target process gone or wrong PID | `is_process_running(pid)` returns false |
| `errors=N` per window after initial success | Out of memory on target node | Check `numastat -m` for the node |
| `skipped_no_vma` very high | Samples concentrated outside anon VMAs (e.g., shared libs) | Reduce `events` to load-heavy or check what addresses are arriving |
| `pages_moved=0, in_place=N` | All targeted regions already there | Working as intended; classifier didn't move thresholds |
