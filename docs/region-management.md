# Region Management

A *region* is a fixed-size, page-aligned, naturally-aligned bin in
the target process's virtual address space. The default size is
2 MiB (matching x86 huge-page boundaries).

## Data model

```cpp
// Per-window snapshot (used by classifier + migrator).
struct Region {
    uint64_t start_addr;
    uint64_t hotness;       // sample count this window
    int      current_node;  // last-known NUMA node, -1 = unknown
    int      target_node;   // classifier output
};

// Persistent metadata, kept across windows.
struct RegionState {
    int current_node;
    int idle_windows;       // consecutive windows with 0 samples
};
```

`RegionManager` holds:

* `m_bucket` — `unordered_map<uint64_t, uint64_t>` of `addr →
  count` for the **current window only**. Written by the sampler
  thread under `m_bucket_mu`.
* `m_state` — `unordered_map<uint64_t, RegionState>` of persistent
  per-region metadata. Touched only by the window thread.

## Address-space binning

A sample address `addr` maps to a region base:

```
base = (addr / region_size) * region_size
```

For `region_size = 2 MiB`, address `0x7fff_abcd_0123` falls into
the region starting at `0x7fff_abc0_0000`.

This is **virtual** binning — completely orthogonal to physical
NUMA layout, file mappings, or huge pages. The kernel handles the
actual page granularity at `move_pages` time.

### Why not align to VMAs?

VMAs are a property of the program, not of the access pattern.
Two adjacent 8-MiB anon mappings should be tier-able independently
in 2-MiB chunks; a 256-MiB heap should also be sub-divided. Fixed
binning gives a uniform unit regardless of mapping topology.

VMAs come back into the picture at *migration* time — see
[migration.md](migration.md) for the VMA-clip step.

## Sample ingestion (sampler thread)

```cpp
void RegionManager::add_sample(uint64_t addr) {
    uint64_t base = (addr / m_region_size) * m_region_size;
    std::lock_guard<std::mutex> lk(m_bucket_mu);
    ++m_bucket[base];   // unordered_map insert/update — O(1) amortized
}
```

A 4 KiB sample at address `addr` increments the count for its
2-MiB-aligned bin. Each call holds the mutex for a single
hash-table operation (~10–100 ns).

## Window snapshot (window thread)

```cpp
std::vector<Region> RegionManager::snapshot_and_swap();
```

Atomic-ish drain:

1. `std::unordered_map<uint64_t,uint64_t> drained;`
2. Lock `m_bucket_mu`, `drained.swap(m_bucket)`, unlock.
3. Sampling continues into a freshly-empty `m_bucket`. No samples
   are dropped beyond the brief lock window.
4. For every entry in `drained`:
   * Look up / create `m_state[addr]`.
   * Reset its `idle_windows = 0`.
   * Emit a `Region` with `current_node` from state.
5. For every persistent state entry that was *not* in `drained`,
   bump `idle_windows`.

The returned snapshot is a `std::vector<Region>` containing only
regions that saw at least one sample this window.

## Address-based current_node update

After `migrate_regions()` returns, the window thread iterates the
snapshot and calls:

```cpp
region_mgr.update_current_node(r.start_addr, r.current_node);
```

This writes the new known-node back into `m_state` keyed by
**address**. The previous design did this by **index**, which was
racy: the sampler could insert a new region (in the middle of the
sorted vector) between snapshot and update, shifting every later
index. Address-based lookup is immune.

## Eviction

```cpp
size_t RegionManager::evict_idle(int max_idle_windows);
```

After every window the daemon calls `evict_idle(cfg.max_idle_windows)`
(default `10`). Any persistent state with `idle_windows >= N` is
dropped. This prevents unbounded growth in long-running daemons.

The threshold is intentionally a multiple of the window — for the
default 20 s window and 10 idle windows, regions silent for 200 s
are forgotten.

## Memory cost

For a workload with 100 K unique regions tracked:

* `m_state`: ~100 K × (8 + 8) bytes payload + ~50 % hash overhead ≈ 2.4 MB
* `m_bucket` (peak): ~100 K × 16 bytes ≈ 1.6 MB

Negligible compared to the workload itself.

## Failure modes

| Symptom | Cause | Fix |
|---------|-------|-----|
| `tracked_count` grows forever | `max_idle_windows` too high or many transient regions | Lower `max_idle_windows`; verify workload doesn't churn addresses |
| Snapshot huge → high latency per window | `region_size` too small for workload | Increase `region_size` (e.g., 4M, 16M) |
| Same region oscillates hot/cold | No hysteresis — see open follow-ups in [design.md](design.md) | Future work: EWMA smoothing |
