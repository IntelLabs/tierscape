# Architecture

`tierscaped` is a single-binary userspace daemon that periodically
migrates pages of a target process between two NUMA nodes (a *hot*
tier and a *cold* tier) based on live PEBS sampling of memory
accesses.

## Component Overview

```
                ┌─────────────────────────────────────────────────────┐
                │                       tierscaped                    │
                │                                                     │
   target ──▶   │   ┌─────────┐    samples    ┌──────────────┐        │
   process      │   │ Sampler │ ────────────▶ │ RegionManager│        │
   PEBS         │   │ (perf)  │  (addr only)  │  (bucketed)  │        │
                │   └────┬────┘               └──────┬───────┘        │
                │        │                           │                │
                │        │  fork+exec child PG       │ snapshot       │
                │        │  setsid; killpg on stop   │ (per window)   │
                │        ▼                           ▼                │
                │   /bin/sh -c "perf record -d ...  ┌──────────────┐  │
                │   | perf script --fields=time,addr"│  Classifier │  │
                │                                    │ (percentile) │  │
                │                                    └──────┬───────┘  │
                │                                           │          │
                │                                           ▼          │
   target  ◀────│                                    ┌──────────────┐  │
   process      │              read /proc/pid/maps  │   Migrator   │  │
   pages        │            ───────────────────────▶│  (move_pages)│  │
   migrate      │                                    └──────────────┘  │
                └─────────────────────────────────────────────────────┘
```

## Processes and Threads

| Entity | Purpose |
|--------|---------|
| `tierscaped` main thread | Window loop: snapshot → classify → migrate |
| sampler thread (in-process) | Reads perf-script stdout line-by-line, calls `RegionManager::add_sample()` |
| sampler child process group | `sh -c "perf record ... \| perf script ..."` in its own session via `setsid()` |
| migrator worker threads | N parallel `std::thread`s issuing `move_pages` syscalls |

The sampler child is started with `setsid()`, so it lives in its own
process group. Shutdown sends `SIGTERM` to the entire PG with
`killpg()`, which kills both `perf record` and `perf script`
atomically — no orphans.

## Window Loop (high-level)

```
loop forever:
    sleep(window_seconds)
    snap = region_mgr.snapshot_and_swap()      # drain sample bucket
    if snap empty: continue

    classify_regions(snap, hot_pct, hot_node, cold_node)

    vmas = read_proc_maps(target_pid)          # fresh VMA list
    stats = migrate_regions(snap, ..., vmas)

    for each region in snap:
        region_mgr.update_current_node(addr, region.current_node)

    region_mgr.evict_idle(max_idle_windows)
```

## Source Tree

| File | Responsibility |
|------|---------------|
| [src/main.cpp](../src/main.cpp) | CLI, signals, daemonization, window loop |
| [src/config.cpp](../src/config.cpp) | TOML loader with unknown-key warnings |
| [src/sanity.cpp](../src/sanity.cpp) | NUMA, perf, PMU, process-access checks |
| [src/sampler.cpp](../src/sampler.cpp) | PEBS pipeline via `fork+setsid`, line parser |
| [src/region.cpp](../src/region.cpp) | Bucketed sample accounting with eviction |
| [src/classifier.cpp](../src/classifier.cpp) | Percentile-based hot/cold split |
| [src/proc_maps.cpp](../src/proc_maps.cpp) | `/proc/<pid>/maps` parser & VMA clipper |
| [src/migrator.cpp](../src/migrator.cpp) | Multi-threaded `move_pages` with budgets |
| [src/util.cpp](../src/util.cpp) | Logging (timestamped), pid file, parsers |

See [design.md](design.md) for the rationale behind each component.
