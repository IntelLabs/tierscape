# Testing

## End-to-end test with `masim`

The repo includes a one-shot driver script:

```bash
sudo bash masim_mod/run_eval.sh
```

What it does:

1. Creates `masim_mod/eval/` (gitignored).
2. Launches `masim` bound to NUMA node 0 with config
   [stairs_4gb_100s](../masim_mod/configs/stairs_4gb_100s)
   — 4 × 1 GiB regions, 25 s per phase (overridable via
   `MASIM_CFG=<name>`; e.g. `MASIM_CFG=stairs_40gb_100s`).
3. Captures pre-tierscaped `numastat`.
4. Starts `tierscaped` in foreground/verbose with
   `--window 10 --hot-pct 75` (aggressive demotion, 10 s windows
   for faster feedback than the 20 s default).
5. Snapshots `numastat -p <PID>` every 10 s for the full run.
6. Sends `SIGTERM` to both, captures the final numastat.

## Output files

All artifacts are written to `masim_mod/eval/` with an ISO-ish
timestamp suffix:

```
masim_mod/eval/
├── run_<TS>.summary        # human-readable: pre/post numastat + log tail
├── tierscaped_<TS>.log     # full daemon stderr (timestamped)
├── numastat_<TS>.log       # per-10s numastat snapshots
└── masim_<TS>.log          # workload stdout
```

## What a successful run looks like

From a real run on this machine
([tierscaped_20260518_133209.log](../masim_mod/eval/)):

```
2026-05-18T08:03:35.488Z [INFO] Window 10: moved  86528 pages (169 regions), 0 in-place, 0 errors, 0 skipped
2026-05-18T08:03:43.632Z [INFO] Window 11: moved  97792 pages (191 regions), 0 in-place, 0 errors, 0 skipped
2026-05-18T08:03:43.633Z [VERB] Evicted 1 idle regions
2026-05-18T08:03:43.683Z [INFO] Shutting down: target exited or stop signal received
2026-05-18T08:03:43.690Z [VERB] Sampler stopped (2406158 samples seen)
2026-05-18T08:03:43.690Z [INFO] tierscaped exited cleanly after 11 windows
```

Numastat went from `Node 0: 4100 MB, Node 1: 2 MB` (pre) to
`Node 0: 2316 MB, Node 1: 1785 MB` (post-shutdown) — confirming
the daemon migrated ~1.8 GiB across 11 windows.

## What to look for

| Indicator | Healthy | Unhealthy |
|-----------|---------|-----------|
| Sample count grows window-to-window | yes (1000s/s typical) | `Window N: no samples` repeatedly → check perf events |
| `pages_moved` non-zero on first few windows | yes (workload starts on hot node) | always 0 → wrong NUMA nodes or process not accessing memory |
| `errors` per window | 0 | high → out of memory on target node, or page-table races |
| `skipped_no_vma` | 0 for anon workloads | high → samples landing in libraries; consider event tuning |
| Final numastat shows split | yes | all-on-hot → migration disabled / pages stuck |
| `Sampler stopped (N samples seen)` on exit | always | missing → sampler thread died unexpectedly |
| `exited cleanly after N windows` | always | missing → daemon crashed |

## Other `masim` configs

`masim_mod/configs/`:

| File | Memory | Use |
|------|--------|-----|
| `stairs_4gb_100s` | 4 GiB (4 × 1 GiB) | Default driver config; 4 × 25 s phases (~100 s) |
| `stairs_4gb_20s_t1` | 4 GiB (4 × 1 GiB) | Fast smoke (4 × 5 s phases, single thread) |
| `stairs_40gb_100s` | 40 GiB (4 × 10 GiB) | Larger-footprint validation |
| `stairs_50gb_10s_t64` | 50 GiB | Multi-threaded stress (64 threads) |

## Manual tier verification

```bash
# Start your workload on the hot node
numactl --membind=0 ./workload &
APP=$!

# Attach tierscaped, foreground+verbose
sudo src/build/tierscaped -f -v -c src/test_config.toml -p $APP &
TS=$!

# Watch migration live (separate terminal)
watch -n 2 "numastat -p $APP"

# Stop
kill -TERM $TS
```

## Troubleshooting failed tests

See [troubleshooting.md](troubleshooting.md) for common error
patterns and fixes.
