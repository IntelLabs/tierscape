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

1. Collect hotness values from **all tracked** regions (including
   silent ones with `hotness = 0`) into a vector. Silent regions are
   included so the percentile is computed over the full tracked
   footprint, not just the ones that happened to fire samples this
   window.
2. `std::sort` ascending.
3. Compute threshold index:
   ```
   idx       = floor((hot_percentile / 100.0) * (N - 1))
   threshold = max(sorted[idx], 1)   // floored at 1
   ```
   The floor at 1 ensures any region that received samples is
   promotable when the hot budget exceeds the active set size.
4. For every region in the snapshot:
   * if `hotness >= threshold`: `target_node = hot_node`
   * else: `target_node = cold_node`

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

## Silent regions (no samples this window)

A region in `m_state` that didn't receive any samples this window
**still appears in the snapshot, with `hotness = 0`**. The
classifier evaluates it like any other region: with `hotness = 0`
it always lands at or below the percentile threshold and gets
classified as cold (`target_node = cold_node`).

This is intentional. It means:

* The percentile is computed against the **full tracked footprint**,
  so a small spike of activity in one region doesn't make the rest
  of the working set look hot just because they're now silent.
* Regions that were once promoted but have since gone quiet are
  re-classified as cold and will be demoted on the next window,
  freeing the hot tier.
* The `idle_windows` counter is still incremented for silent
  regions; after `max_idle_windows` they are evicted from `m_state`
  entirely (see [region-management.md](region-management.md)).

## Known limitations

* **No hysteresis.** A region near the threshold can ping-pong
  between hot and cold across consecutive windows. Future work:
  EWMA-smoothed hotness, or two thresholds (hot + cold) with a gap.
* **No size-weighting.** A 2-MiB region with 10 samples is treated
  the same as a 2-MiB region with 10 samples elsewhere even if one
  has 1 mapped page and the other has 512. With VMA filtering at
  migration time this is mostly a wash, but a size-weighted
  classifier could be more efficient.
