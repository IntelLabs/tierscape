# Classification

The classifier splits a window's sampled regions into **hot** and
**cold** based on a percentile threshold over per-region sample
counts.

## Algorithm

```cpp
void classify_regions(std::vector<Region>& regions,
                      float hot_percentile,
                      int hot_node,
                      int cold_node);
```

1. Collect `hotness > 0` values from the snapshot into a vector.
2. `std::sort` ascending.
3. Compute threshold index:
   ```
   idx = floor((hot_percentile / 100.0) * (N - 1))
   threshold = sorted[idx]
   ```
4. For every region in the snapshot:
   * if `hotness >= threshold && threshold > 0`:
     `target_node = hot_node`
   * else:
     `target_node = cold_node`

## Semantics of `hot_percentile`

`hot_percentile` is the **percentile below which regions are
demoted**. Some examples for a window with 100 sampled regions:

| `hot_percentile` | Threshold position | Result |
|------------------|-------------------|--------|
| `25` (default) | 25th percentile | ~75 % stay hot, bottom 25 % demoted |
| `50` | median | top 50 % hot, bottom 50 % demoted |
| `75` (aggressive) | 75th percentile | only top 25 % hot, 75 % demoted |
| `0` | minimum | everything hot (nothing demoted unless hotness=0) |
| `100` | maximum | only the single hottest region stays hot |

In practice the default `25` is conservative — useful when the hot
tier has plenty of headroom. Use `75` when the hot tier is small
relative to the working set.

## Why percentile?

Absolute thresholds (e.g., "anything < 100 samples is cold") are
brittle:

* Sample rate depends on `frequency` and workload memory
  intensity. A workload that's 10× more memory-bound will trip the
  threshold differently.
* Phase changes (e.g., warm-up vs. steady-state) shift the
  distribution.

A percentile threshold normalizes against the current window's
distribution. It always promotes the *relatively* hottest regions
and demotes the *relatively* coldest, regardless of absolute rate.

## Tie handling

If multiple regions share the threshold value, all of them are
classified hot (because `hotness >= threshold`). This can produce a
hot/cold split slightly different from the requested percentile,
especially with low sample counts. With ~100 + samples per region
this drift is negligible.

## Regions absent from this window

A region in `m_state` that didn't receive any samples this window
does **not** appear in the snapshot. The classifier never sees it,
and the migrator never touches it. Its persistent `current_node`
stays as last-known, and its `idle_windows` counter is incremented
(see [region-management.md](region-management.md)).

This means a region demoted in window N stays on the cold tier
unless it shows up in window N+1 hot enough to clear the
threshold — i.e., the daemon does not periodically re-test cold
regions. That is intentional: cold regions are silent, so they
should stay where they are with no PEBS overhead.

## Known limitations

* **No hysteresis.** A region near the threshold can ping-pong
  between hot and cold across consecutive windows. Future work:
  EWMA-smoothed hotness, or two thresholds (hot + cold) with a gap.
* **No size-weighting.** A 2-MiB region with 10 samples is treated
  the same as a 2-MiB region with 10 samples elsewhere even if one
  has 1 mapped page and the other has 512. With VMA filtering at
  migration time this is mostly a wash, but a size-weighted
  classifier could be more efficient.
