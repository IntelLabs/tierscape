# masim — Memory Access Simulator

A configurable memory access simulator used for testing and validating `tierscaped`.

Supports multithreaded **load** (first-touch `memset`) and **access** (read/write)
phases. Per-phase throughput is reported as the sum of ops across all worker
threads.

## Build

```bash
make
```

## Run

```bash
./masim configs/<config file>
```

## Config File Format

A config file is composed of three paragraphs separated by blank lines.
Lines starting with `#` are comments.

```
<nr_threads>            # optional; first paragraph; default = 1

<region_name>, <size_bytes>
<region_name>, <size_bytes>
...

phase <name>
<duration_ms>
<region_name>, <random_access 0|1>, <stride_bytes>, <probability>, <ro|wo|rw>
...

phase <name>
...
```

### Thread count (first paragraph)

The first non-comment paragraph may contain a single integer — the number of
worker threads used for region initialization and for each phase's access loop.

- If present (no comma on the line), it is consumed as `nr_threads`.
- If absent (first paragraph already looks like regions, i.e. contains a comma),
  `nr_threads` defaults to `1`. Existing single-threaded configs work unchanged.

Example with 64 threads — see [`configs/stairs_50gb_10s_t64`](configs/stairs_50gb_10s_t64):

```
4

s0, 1073741824
s1, 1073741824
s2, 1073741824
s3, 1073741824

phase 0
5000
s0, 0, 4096, 100, ro
...
```

## Throughput report

After each phase masim prints per-thread ops plus an aggregated line:

```
THREAD 0 ops=288,620,544 time_us=5,010,246
THREAD 1 ops=288,882,688 time_us=5,010,199
...
REGION_TIME 5,010,628 us. Total Ops: 1,154,744,320
REGION_TIME_SEC 5.010628
PHASE_THROUGHPUT ops=1,154,744,320 time_us=5,010,628 thp=230,459,000.35 ops/s threads=4
```

`PHASE_THROUGHPUT.ops` is the **sum across all threads**; `thp` is `ops / wall_seconds`.

## Available Configs

| Config | Total Memory | Regions | Threads | Duration | Purpose |
|--------|-------------|---------|---------|----------|---------|
| `stairs_4gb_100s` | 4 GB | 4 × 1 GB | 1 | ~100 s | Default driver config (4 × 25 s phases) |
| `stairs_4gb_20s_t1` | 4 GB | 4 × 1 GB | 1 | ~20 s | Fast smoke (4 × 5 s phases) |
| `stairs_40gb_100s` | 40 GB | 4 × 10 GB | 1 | ~100 s | Larger-footprint validation |
| `stairs_50gb_10s_t64` | 50 GB | varies | 64 | ~10 s | Multi-threaded stress |

## Multithreaded scaling test

Bind CPU and memory to a single NUMA node and compare 1 vs many threads:

```bash
numactl -N 0 -m 0 ./masim configs/stairs_4gb_20s_t1
numactl -N 0 -m 0 ./masim configs/stairs_50gb_10s_t64
```

Throughput should scale roughly linearly with thread count up to
the node's memory bandwidth ceiling. The exact numbers depend on
the host — a representative single-NUMA run on a Xeon Gold 6554S
showed ~3.4× scaling from 1 to 4 threads on 4 × 1 GiB sequential
RO regions.

## Testing with tierscaped

Bind masim to one NUMA node, then let `tierscaped` migrate cold pages:

```bash
# Start masim on node 0
numactl --membind=0 ./masim configs/test_tier_4gb_long

# In another terminal, attach tierscaped
sudo ../src/build/tierscaped -f -v -c ../tierscaped.toml -p $(pgrep masim)

# Watch migration in a third terminal
watch -n 2 "numastat -p $(pgrep masim)"
```

## Notes / limitations

- All worker threads access the same region with the same pattern per phase
  (N readers on shared data). Each worker keeps its own `last_offset` / `last_page`
  cursor — no shared writes in the hot path.
- RNG (`rndint()`) is thread-safe via `__thread` cursors over a read-only table
  populated by `init_rndints()`.
- In time-mode (default) all worker threads stop at the same `phase->time_ms`
  budget. In `OPS_MODE`, total loops are split evenly across threads (remainder
  goes to thread 0).

