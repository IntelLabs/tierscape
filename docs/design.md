# Design Rationale

## Goals

1. **Single binary, no orchestration.** The previous TierScape used
   shell scripts, an ILP server, and a custom kernel. Operationally
   complex and brittle. This rewrite is one C++17 binary.
2. **Stock kernel.** No custom syscalls. Only `move_pages(2)` and
   standard PEBS via `perf`.
3. **Two byte-addressable tiers only.** No compressed tier (zswap,
   IAA, etc.) — they introduced syscall 452 dependencies and
   complicated the migration path. They can be added back later as a
   plug-in tier-driver, but not in the core.
4. **Generic.** No hard-coded paths, NUMA node IDs, perf-event hex,
   or workload assumptions. Everything is in [configuration.md](configuration.md).

## Key Design Decisions

### D1. Daemon owns the entire perf pipeline

Earlier prototypes ran `perf record | perf script` from shell. That
made shutdown ordering racy (the shell, perf-record, perf-script,
the daemon, and the workload all had to die in the right order).

Now the daemon:

1. `pipe(pipefd)` — create the read end.
2. `fork()` — child calls `setsid()`, redirects stdout to the pipe,
   `execl("/bin/sh", "sh", "-c", "perf record ... | perf script ...")`.
3. Parent reads from `pipefd[0]`, parses line-by-line.
4. Shutdown: `killpg(child_pgid, SIGTERM)`, then `waitpid` with a
   short SIGKILL fallback.

This guarantees no orphaned `perf` processes and no zombie
descendants, even on crash.

### D2. Sample collection is double-buffered

The sampler thread inserts into an `unordered_map<addr,count>`
behind a small mutex. The window thread, once per
`window_seconds`, swaps the map out under the same lock — sampling
resumes against an empty map in O(1) and the window thread
processes the drained map outside the critical section.

Compared to the previous design (a sorted vector with O(n)
inserts), this scales to millions of samples/sec.

### D3. Persistent region state, ephemeral hotness

`RegionManager` keeps two tables:

* **Bucket** (`unordered_map<addr, hotness>`) — *current window only*. Drained each window.
* **State** (`unordered_map<addr, RegionState>`) — *persistent*. Tracks `current_node` (last-known NUMA placement) and `idle_windows`.

The snapshot returned by `snapshot_and_swap()` joins the two:
hotness from the bucket, current_node from the persistent state.

### D4. Cold regions are evicted

A region that goes `max_idle_windows` consecutive windows with zero
samples is dropped from the state table. Without this, long-running
daemons accumulate the entire access history forever — memory grows
unbounded.

### D5. Migration intersects with /proc/<pid>/maps

`move_pages()` accepts any virtual address but quietly returns
`-ENOENT` in `status[]` for pages not mapped. Without VMA
filtering, every 2 MiB region issued one `move_pages` call covering
512 pages, most often with the majority unmapped — wasted syscalls.

`proc_maps.cpp` reads `/proc/<pid>/maps` once per window and
[`clip_to_migratable_vma`](../src/proc_maps.cpp) trims each
region's `[start,end)` to the first overlapping anon-writable
VMA. Regions with no overlap are counted in
`MigrateStats::skipped_no_vma` and never reach `move_pages`.

### D6. Address-based update after migration

After a migration window, the daemon walks the snapshot and calls
`region_mgr.update_current_node(start_addr, new_node)`. The lookup
is by address, not by index — robust against intervening sampler
inserts that would shift the persistent table.

### D7. CLI parsing validates aggressively

`atoi("abc")` returns 0 silently. We use `strtol`/`strtod` with
full-consumption checks and explicit range bounds (e.g., NUMA node
0–1023, threads 1–1024). Invalid input is rejected at startup, not
silently defaulted.

### D8. Logging includes ISO-8601 UTC timestamps

Daemons that write to systemd journals or files without timestamps
are unanalyzable. Every log line is now prefixed:

```
2026-05-18T07:59:36.857Z [INFO] tierscaped started: ...
```

### D9. Signal handling uses `sigaction`

`signal()` semantics vary across implementations. `sigaction` with
`SA_RESTART` is portable and well-defined. We install handlers for:

* `SIGTERM`, `SIGINT` → flip `g_running` atomic
* `SIGCHLD` → flip `g_child_exited` flag (sampler / launched child)
* `SIGPIPE` → ignored (perf pipeline closes asynchronously)

### D10. Migrator has no global state

The previous migrator had file-scope `std::atomic<>` counters and a
start-time, making it non-reentrant. The rewrite passes a
heap-local `MigCtx` struct to each worker thread — concurrent
invocations would be safe and there is no static-init-order risk.

## What is intentionally *not* in scope

* **Hot promotion from cold tier on re-access** — a region accessed
  on the cold node will be classified hot and promoted naturally
  on the next window; we do not handle PEBS source-data accesses
  separately.
* **CGroup-based process selection** — one PID per daemon for now.
  Run multiple daemons for multiple targets.
* **THP awareness** — `move_pages` accepts head-page or constituent
  4 KiB addresses; the kernel handles THP correctly. The daemon
  uses `sysconf(_SC_PAGESIZE)` for stride.
* **Metrics export** — verbose logging only; add Prometheus later
  if needed.

## Open follow-ups (not implemented)

* EWMA over hotness across windows to reduce thrash.
* Pagemap-based pre-check to skip absent pages without invoking
  `move_pages`.
* Per-VMA region size (so large heaps get coarse regions and small
  mappings get fine ones).
