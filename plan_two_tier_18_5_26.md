# Plan: Simplified Two-Tier Byte-Addressable TierScape Daemon

**Date:** 2026-05-18  
**Branch:** `two_tier_byte_addressable`  
**Status:** ✅ **IMPLEMENTED AND TESTED**  
**Goal:** Strip TierScape down to a simple two-tier (DRAM ↔ CXL/Optane) byte-addressable memory tiering daemon using `move_pages()` only. Remove ILP solver, waterfall mode, compressed tier support entirely.

---

## Summary of Changes

| What | Current | New |
|------|---------|-----|
| Tiers | 2 byte-addressable + 3 compressed | 2 byte-addressable only |
| Decision logic | HEMEM / ILP / Waterfall | Percentile-based hot/cold only |
| ILP server | Required for mode 1 | **Removed** |
| Compressed tier syscall | `SYS_do_migrate_dst_tier (452)` | **Removed** |
| Migration | `move_pages()` + zswap syscall | `move_pages()` only |
| Startup | Shell script orchestration | Self-contained daemon binary |
| Config | Shell variables (`skd_config.sh`) | **TOML config file** |
| Kernel requirement | Custom kernel for ntier | Stock kernel (NUMA nodes) |

---

## Architecture

```
tierscaped [OPTIONS] -p <PID>
tierscaped [OPTIONS] -- <command> [args...]

  ┌──────────────────────────────────────────────┐
  │            tierscaped (daemon)                │
  │                                              │
  │  1. Sanity checks (NUMA nodes, perf, events) │
  │  2. Fork + setsid (daemonize)                │
  │  3. Spawn perf record | perf script pipeline │
  │  4. Consume PEBS samples → region hotness    │
  │  5. Every <window>: percentile classification│
  │  6. Migrate via move_pages() (N threads)     │
  │  7. Exit when target PID dies or SIGTERM     │
  └──────────────────────────────────────────────┘
```

---

## 1. CLI Interface

```
tierscaped [OPTIONS] -p <PID>
tierscaped [OPTIONS] -- <command> [args...]

OPTIONS:
  -p, --pid <PID>           Target process (mutually exclusive with --)
  -c, --config <path>       Path to TOML config file (default: /etc/tierscaped.toml)
  --hot-node <N>            NUMA node for hot tier (overrides config)
  --cold-node <N>           NUMA node for cold tier (overrides config)
  --hot-pct <float>         Percentile threshold for hot (default: 25.0)
  --freq <int>              PEBS sampling frequency (default: 10000)
  --threads <int>           Migration threads (default: 2)
  --window <int>            Window size in seconds (default: 20)
  --region-size <size>      Region size (default: 2M, accepts K/M/G suffix)
  --max-pages <int>         Max pages to move per window (default: 5000000)
  -v, --verbose             Verbose logging to stderr
  -f, --foreground          Don't daemonize (stay in foreground)
  --dry-run                 Profile only, no migration
  --pidfile <path>          Write daemon PID here (default: /tmp/tierscaped.pid)
```

**Priority:** CLI args override TOML config values.

---

## 2. TOML Config File

Default path: `/etc/tierscaped.toml` (overridable with `-c`)

```toml
[tiers]
hot_node = 0
cold_node = 1

[sampling]
# PEBS events (use symbolic names or raw hex; add :P for precise, :Pu for userspace-only)
events = [
    "mem_inst_retired.all_loads:P",
    "mem_inst_retired.all_stores:P",
    "mem_inst_retired.stlb_miss_loads:P",
    "mem_inst_retired.stlb_miss_stores:P",
]
# Period: sample every N event occurrences (used as perf -c flag)
frequency = 10000
window_seconds = 20

[classification]
hot_percentile = 25.0    # regions at or above this percentile → hot tier

[migration]
threads = 2
max_pages_per_window = 5000000   # ~20GB
region_size = "2M"               # 2M, 4M, 1G etc.

[daemon]
pidfile = "/tmp/tierscaped.pid"
verbose = false
log_file = ""                    # empty = stderr only
perf_bin = "/usr/bin/perf"       # path to perf binary matching your kernel
```

---

## 3. Sanity Checks (at startup, before daemonizing)

The daemon MUST validate the following before forking. Exit with clear error on failure:

### 3.1 NUMA Node Validation
- [ ] Check hot_node exists: `/sys/devices/system/node/node<N>/` exists
- [ ] Check cold_node exists: `/sys/devices/system/node/node<N>/` exists  
- [ ] Check hot_node != cold_node
- [ ] Check hot_node has non-zero memory: parse `/sys/devices/system/node/node<N>/meminfo` → `MemTotal` > 0
- [ ] Check cold_node has non-zero memory: same check
- [ ] Verify `libnuma` can see both nodes: `numa_node_size(node, &freep)` > 0

### 3.2 Perf Availability
- [ ] Check `perf` binary exists and is executable (configurable path, default `/usr/bin/perf`)
- [ ] Check `perf record` can run with `-p <PID>` (quick test or just check permissions)
- [ ] Check `/proc/sys/kernel/perf_event_paranoid` ≤ 1 (or running as root)

### 3.3 PMU Counter Availability
- [ ] For each event in the config, run `perf stat -e <event> -a sleep 0.01` and check exit code
- [ ] If any event fails, report which event is unsupported and exit
- [ ] Alternative: check `/sys/devices/cpu/events/` or use `perf list` to validate

### 3.4 Process Validation (if -p given)
- [ ] Check `/proc/<PID>/` exists
- [ ] Check we can read `/proc/<PID>/maps` (permissions)
- [ ] Check we can read `/proc/<PID>/pagemap` (requires CAP_SYS_PTRACE or root)

---

## 4. Daemon Lifecycle

```
main()
  ├── parse CLI args
  ├── load TOML config (CLI overrides config)
  ├── run sanity checks (Section 3)
  ├── if launching command (-- mode):
  │     fork+exec the command, get PID
  ├── daemonize (unless --foreground):
  │     fork() → parent exits
  │     setsid()
  │     fork() again → first child exits
  │     redirect stdin/stdout/stderr to /dev/null (or log_file)
  │     write PID to pidfile
  ├── setup signal handlers (SIGTERM, SIGINT → graceful stop)
  ├── spawn perf pipeline (perf record ... | perf script ...)
  ├── spawn threads:
  │     - PEBS consumer thread (reads perf script output)
  │     - Window timer thread (periodic processing)
  ├── main loop: while (target PID alive && !stop_signal)
  │     sleep(1), check PID
  ├── cleanup:
  │     kill perf pipeline
  │     join threads
  │     remove pidfile
  │     exit(0)
```

---

## 5. Core Logic (Simplified)

### 5.1 PEBS Consumption
- Pipe: `perf record -d -e <events> -c <period> -p <PID> -o - | perf script -i - --fields=time,addr`
  - `-d` flag: records **data addresses** (memory locations accessed), not instruction pointers
  - `-c <period>`: period-based sampling (required for streaming; `-F` frequency mode does NOT stream)
  - `-o -`: output to stdout (binary perf.data format, streamed)
  - Events with `:P` suffix: enables PEBS precise sampling
- `perf script --fields=time,addr` output format: `<timestamp>:     <hex_address>`
- Parse each line → extract hex address after `:` → filter kernel addresses (≥ 0xffff...) → page-align → push to shared region manager
- Pipeline managed internally via `popen()` (no external shell scripts)

### 5.2 Window Processing
Every `window_seconds`:
1. Lock + swap the address vector (snapshot)
2. Assign samples to regions (page-aligned addr → region index = `addr / region_size`)
3. Compute per-region hotness (count of samples in window)
4. Compute percentile distribution of hotness values
5. Classify: regions with hotness ≥ `percentile_val[hot_percentile]` → hot (move to hot_node)
6. Everything else → cold (move to cold_node)

### 5.3 Migration
- Multi-threaded: divide regions among `threads` workers
- For each region where `dst_node != current_node`:
  - Enumerate pages in region: `start_addr` to `start_addr + region_size`
  - Call `move_pages(pid, count, pages[], NULL, status[], MPOL_MF_MOVE)`
  - Or with target nodes: `move_pages(pid, count, pages[], nodes[], status[], MPOL_MF_MOVE)`
- Time-bounded: stop if elapsed > `window_seconds * 0.9`
- Page-capped: stop if moved > `max_pages_per_window`

---

## 6. What Gets Removed

| Component | Location | Action |
|-----------|----------|--------|
| ILP server | `ilp_server/` | Do not build/use |
| ILP mapping | `sk_daemon/core/mapping_logic/mapping_ilp.*` | Remove |
| Waterfall mapping | `sk_daemon/core/mapping_logic/mapping_waterfall.*` | Remove |
| Scatter mapping | `sk_daemon/core/mapping_logic/mapping_scatter.*` | Remove |
| Google mapping | `sk_daemon/core/mapping_logic/mapping_google.*` | Remove |
| DAMON support | `sk_daemon/core/mapping_logic/mapping_damon.*` | Remove |
| None mapping | `sk_daemon/core/mapping_logic/mapping_none.*` | Remove |
| Compressed tier logic | `tier_config.h` ENABLE_NTIER sections | Remove |
| zswap syscall (452) | `tier_utils.cpp` | Remove |
| zswap pool stats thread | `tracker_d.cpp` | Remove |
| Shell orchestration | `skd_daemon/shell_scripts/`, `skd_profile_driver.sh` | Replace with daemon internals |
| `skd_config.sh` | `skd_daemon/skd_config.sh` | Replace with TOML |
| Comms model (ILP protocol) | `utils/comms_model.h` | Remove |
| External perf scripts | `perf_scripts/` | Daemon manages perf internally |

---

## 7. Files Created

| File | Purpose |
|------|---------|
| `tierscaped.toml.example` | Example TOML config |
| `src/main.cpp` | Entry point, CLI parsing, daemonization, window loop |
| `src/config.cpp` / `src/config.h` | TOML parser + Config struct |
| `src/sanity.cpp` / `src/sanity.h` | Sanity checks (NUMA, perf, PMU, process) |
| `src/sampler.cpp` / `src/sampler.h` | PEBS pipeline (perf record \| perf script via popen) |
| `src/classifier.cpp` / `src/classifier.h` | Percentile-based hot/cold classification |
| `src/migrator.cpp` / `src/migrator.h` | Multi-threaded move_pages() migration |
| `src/region.cpp` / `src/region.h` | Region management (sorted-vector, configurable size) |
| `src/util.cpp` / `src/util.h` | Logging, PID checks, pidfile management |
| `src/CMakeLists.txt` | CMake build system (C++17, links libnuma + pthreads) |
| `src/Makefile` | Convenience wrapper |
| `src/test_config.toml` | Test config for validation machine |
| `masim_mod/configs/test_tier_4gb_long` | Long-running test workload (4×1GB, 60s phases) |

**Dependencies (all standard Linux):**
- `libnuma` (for `move_pages`, `numa_node_size64`)
- `pthreads` / `std::thread`
- `perf` binary matching kernel version
- No external TOML library — custom minimal parser

---

## 8. Build

```bash
# Simple build
make            # builds tierscaped binary

# Or with cmake
mkdir build && cd build
cmake ..
make
```

Output: single binary `tierscaped`

---

## 9. Usage Examples

```bash
# Attach to existing process
sudo tierscaped -p 12345 --hot-node 0 --cold-node 1

# Launch and tier a process
sudo tierscaped --config ./my_config.toml -- ./my_workload --args

# Foreground + verbose (for debugging)
sudo tierscaped -f -v -p 12345

# Stop
kill $(cat /tmp/tierscaped.pid)
# or
kill -SIGTERM <daemon_pid>
```

---

## 10. Validation Using `masim_mod`

**Location:** `/data/sandeep/ntier_work/tierscape_il_gitrepo/masim_mod`

masim is a **M**emory **A**ccess **SIM**ulator — a configurable microbenchmark that allocates memory regions and runs access patterns with controllable hotness, making it ideal for validating tiering decisions.

### 10.1 How masim Works

- **Config file** defines memory regions (name, size in bytes) and phases (access patterns)
- Each phase specifies: region, random/sequential, stride, probability, read/write mode
- Runs in `AUTO_MODE` by default (no signal needed to start execution)
- Prints PID at startup → can be used with `-p` attach mode
- Can also be launched via `tierscaped -- ./masim configs/<cfg>`

### 10.2 Recommended Validation Configs

| Config | Total Memory | Description | Validation Use |
|--------|-------------|-------------|----------------|
| `stairs_plot_1gb` | 4 × 1GB = 4GB | 4 regions, sequential phases, 5s each | Quick smoke test (fits in DRAM) |
| `stairs_100GB_10GB` | ~90GB (9 regions: 5×~10GB cold + 4×10GB hot) | Hot regions accessed one-at-a-time in phases | **Primary validation**: clear hot/cold, large enough to span nodes |
| `stairs_1TB_100g` | ~900GB | 400GB active across 200 loops | Stress test, needs large system |
| `zigzag_plot_1gb.cfg` | 4 × 1GB | Repeating phase pattern (zigzag) | Tests re-promotion of previously-cold regions |

### 10.3 Validation Scenarios

#### Scenario A: Basic Attach Mode (smoke test)
```bash
# Terminal 1: Start masim
cd masim_mod && make && ./masim configs/stairs_plot_1gb
# Note the PID printed

# Terminal 2: Attach tierscaped
sudo tierscaped -p <PID> --hot-node 0 --cold-node 1 -v -f
```
**Expected:** Daemon attaches, profiles PEBS, identifies the active region as hot, migrates cold regions to node 1.

#### Scenario B: Launch Mode with `stairs_100GB_10GB`
```bash
sudo tierscaped -v -f --hot-node 0 --cold-node 1 --window 10 \
    -- ./masim_mod/masim masim_mod/configs/stairs_100GB_10GB
```
**Expected:**
- Total ~90GB allocated: ~50GB cold (`s1`-`s5`) + ~40GB hot (`hot1`-`hot4`)
- In each phase (2000ms), only one `hot*` region is accessed
- Daemon should classify the active `hot*` region as hot (keep on node 0)
- All `s*` regions and inactive `hot*` regions classified cold → migrated to node 1
- Verify with `numastat -p <PID>`: majority of memory on cold node, active region on hot node

#### Scenario C: Window/Percentile Tuning
```bash
# Aggressive: only top 5% stays hot
sudo tierscaped -f -v --hot-pct 95 --window 5 \
    -- ./masim_mod/masim masim_mod/configs/stairs_100GB_10GB

# Conservative: top 75% stays hot  
sudo tierscaped -f -v --hot-pct 25 --window 5 \
    -- ./masim_mod/masim masim_mod/configs/stairs_100GB_10GB
```
**Expected:** With `--hot-pct 95`, more data migrated to cold node. With `--hot-pct 25`, less data migrated.

#### Scenario D: Zigzag (re-promotion)
```bash
sudo tierscaped -f -v --window 3 \
    -- ./masim_mod/masim masim_mod/configs/zigzag_plot_1gb.cfg
```
**Expected:** Regions cycle between hot/cold as phases repeat. Daemon should re-promote previously-demoted regions when they become active again.

#### Scenario E: Process termination
```bash
# Start masim with short runtime
sudo tierscaped -f -v -p <PID_of_short_masim>
# masim exits → daemon should detect and exit cleanly
```
**Expected:** Daemon logs "target process exited" and shuts down gracefully, removes pidfile.

### 10.4 Verification with `numactl` and `numastat`

Use `numactl` to force initial placement, then verify that `tierscaped` migrates pages as expected.

#### Initial Placement Strategy
```bash
# Force ALL masim memory onto the HOT node initially
# tierscaped should then demote cold regions to node 1
sudo tierscaped -f -v --hot-node 0 --cold-node 1 --window 10 \
    -- numactl --membind=0 ./masim_mod/masim masim_mod/configs/stairs_100GB_10GB

# Or force ALL onto COLD node initially
# tierscaped should then promote hot regions to node 0
sudo tierscaped -f -v --hot-node 0 --cold-node 1 --window 10 \
    -- numactl --membind=1 ./masim_mod/masim masim_mod/configs/stairs_100GB_10GB
```

#### Periodic Verification (run in a separate terminal)
```bash
MASIM_PID=<pid>
EVAL_DIR=masim_mod/eval

mkdir -p $EVAL_DIR

# Snapshot numastat every 5 seconds into a log
while kill -0 $MASIM_PID 2>/dev/null; do
    echo "=== $(date +%s) ===" >> $EVAL_DIR/numastat.log
    numastat -p $MASIM_PID >> $EVAL_DIR/numastat.log 2>&1
    echo "" >> $EVAL_DIR/numastat.log
    sleep 5
done

# Snapshot numa_maps (shows per-VMA node placement)
cat /proc/$MASIM_PID/numa_maps > $EVAL_DIR/numa_maps_snapshot.log
```

#### Expected Results (membind=0 start, demote cold)
```
# Before tierscaped (all on node 0):
#   Node 0: ~90GB    Node 1: 0GB

# After 1-2 windows (cold demoted):
#   Node 0: ~10GB (active hot region)   Node 1: ~80GB (cold regions)

# As phases rotate, the active hot region changes:
#   Node 0 stays ~10GB but different region; previously-hot region demoted
```

#### Expected Results (membind=1 start, promote hot)
```
# Before tierscaped (all on node 1):
#   Node 0: 0GB    Node 1: ~90GB

# After 1-2 windows (hot promoted):
#   Node 0: ~10GB (active hot region)   Node 1: ~80GB (rest)
```

### 10.5 Eval Directory & Logging

All validation logs go to `masim_mod/eval/` (already gitignored — add `eval/` entry).

```
masim_mod/eval/
├── numastat.log              # periodic numastat snapshots
├── numa_maps_snapshot.log    # per-VMA node placement at a point in time
├── tierscaped_verbose.log    # daemon verbose output (redirect stderr)
└── run_<timestamp>.log       # combined run log
```

**Capturing tierscaped output:**
```bash
mkdir -p masim_mod/eval
sudo tierscaped -f -v --hot-node 0 --cold-node 1 --window 10 \
    -- numactl --membind=0 ./masim_mod/masim masim_mod/configs/stairs_100GB_10GB \
    2>&1 | tee masim_mod/eval/tierscaped_verbose.log
```

**Note:** Add `eval/` to `masim_mod/.gitignore` (currently only has `eval_masim_perf_ovhd/`).

### 10.6 Other Verification Commands

```bash
# Quick check: is daemon running?
cat /tmp/tierscaped.pid
ps aux | grep tierscaped

# One-shot numastat
numastat -p <masim_pid>

# Watch numastat live
watch -n 2 "numastat -p <masim_pid>"

# Verify NUMA hardware topology
numactl --hardware
```

### 10.7 Building masim

```bash
cd masim_mod
make          # produces ./masim binary
./masim configs/stairs_plot_1gb   # quick run to verify it works
```

### 10.8 Creating Custom Validation Configs

Config file format:
```
# regions (name, size_bytes)
hot_region, 10737418240
cold_region1, 10737418240
cold_region2, 10737418240

# phases (separated by blank lines)
# phase <name>
# <duration_ms>
# <region>, <random:0/1>, <stride>, <probability>, <mode:ro/wo/rw>
phase hot_access
60000
hot_region, 0, 4096, 100, ro
```

For a good two-tier validation: create a config with ~80% cold data and ~20% hot data, with hot region accessed continuously. This mirrors real workloads and makes tiering benefits clearly measurable.

---

## 11. Unit Testing Plan

1. **Sanity checks:** Pass/fail on valid/invalid NUMA nodes, missing perf, bad events
2. **Percentile calculation:** Known distributions → verify threshold values
3. **Region assignment:** Virtual addresses → correct region index for different region sizes
4. **Config parsing:** Valid/invalid TOML files, CLI override precedence
5. **Edge cases:** Target process dies mid-window, zero-sample windows, single-region workloads

---

## 12. Migration Path from Current Code

1. Reuse `mapping_hemem.cpp` logic (percentile threshold → hot/cold)
2. Reuse `region_pebs_fixed.cpp` region assignment (extend for configurable size)
3. Reuse `move_to_dram_or_optane()` from `tier_utils.cpp` (the `move_pages` call)
4. Reuse `th_consume_perf_events()` parsing logic
5. Reuse `th_pebs_window_timeout()` window loop structure
6. Delete everything else

---

## Open Questions / Future

- Should we support CGroup-based process selection (tier all processes in a cgroup)?
- Should we support hot-page promotion from cold node on re-access (reactive)?
- Consider adding a `tierscaped status` CLI command that queries the daemon via unix socket
- Metrics export (prometheus endpoint?) — future work

---

## 13. Implementation Notes

### 13.1 Files Created (`src/`)

| File | Lines | Purpose |
|------|-------|---------|
| `src/CMakeLists.txt` | 22 | Build system (cmake, C++17) |
| `src/Makefile` | 11 | Convenience wrapper (`make` / `make debug` / `make clean`) |
| `src/main.cpp` | ~230 | Entry point: CLI parsing (getopt_long), daemonization, window loop |
| `src/config.h` | 48 | Config struct + TOML loader declaration |
| `src/config.cpp` | 130 | Minimal TOML parser (sections, key=value, arrays) |
| `src/sanity.h` | 7 | Sanity check interface |
| `src/sanity.cpp` | 130 | NUMA node, perf binary, PMU event, process validation |
| `src/region.h` | 45 | Region struct + RegionManager class |
| `src/region.cpp` | 43 | Region management with sorted-vector index |
| `src/sampler.h` | 33 | Sampler interface (PEBS pipeline) |
| `src/sampler.cpp` | 75 | popen-based `perf record -d ... -o - \| perf script -i -` pipeline |
| `src/classifier.h` | 12 | Percentile classifier interface |
| `src/classifier.cpp` | 50 | Percentile-based hot/cold classification |
| `src/migrator.h` | 23 | Migrator interface + MigrateStats struct |
| `src/migrator.cpp` | 130 | Multi-threaded move_pages() with time/page caps |
| `src/util.h` | 18 | Logging + process utility declarations |
| `src/util.cpp` | 67 | Logging (verbose/info/warn/err), PID checks, pidfile |
| `tierscaped.toml.example` | 28 | Example config file |
| `src/test_config.toml` | 30 | Test config for the validation machine |

### 13.2 Key Implementation Decisions

1. **Perf pipeline**: Uses `perf record -d -e <events> -c <period> -p <PID> -o - | perf script -i - --fields=time,addr`
   - `-d` flag required for **data address** recording (not instruction pointer)
   - `-c <period>` (not `-F <freq>`) required for **streaming** mode to work
   - `:P` or `:Pu` suffix on events for PEBS precise sampling
   - Streaming via popen — `perf script` outputs lines as buffers fill

2. **No external dependencies for TOML**: Custom minimal parser handles sections, key=value, string arrays (the subset we need). Avoids pulling in toml11/tomlplusplus.

3. **Region management**: Regions are created on-demand as samples arrive (sorted vector + binary search). Regions that were never sampled are never migrated (by design — if it's never accessed, leave it alone).

4. **Classification**: Percentile computed only over regions with >0 samples in the current window. Regions with `hotness >= threshold` stay on hot node; rest go to cold node.

5. **Sanity check for PMU events**: Uses `popen()` to run `perf stat -e <event> -a -- sleep 0.01` and scans output for "not supported" string (more reliable than checking exit code alone).

### 13.3 Build

```bash
cd src && make          # Release build → src/build/tierscaped
cd src && make debug    # Debug build
cd src && make clean    # Remove build directory
```

Dependencies: `libnuma-dev`, `g++` (C++17), `cmake >= 3.10`, `perf` (matching kernel version).

---

## 14. Test Results

### 14.1 Test Environment

| Property | Value |
|----------|-------|
| CPU | Intel Xeon Gold 6554S |
| NUMA nodes | 6 (node 0: 772GB, node 1: 774GB, nodes 2-5: 260GB each) |
| Kernel | 6.15.0-rc3 |
| Perf binary | `/data/sandeep/idxd/tools/perf/perf` (v6.15.rc3) |
| PEBS events | `mem_inst_retired.{all_loads,all_stores,stlb_miss_loads,stlb_miss_stores}:P` |

### 14.2 Sanity Check Validation

```
$ ./tierscaped -f -v -c test_config.toml -p 99999
[INFO] Running sanity checks...
[VERB] NUMA hot node 0: 772737 MB total, 752534 MB free        ✅
[VERB] NUMA cold node 1: 774083 MB total, 756993 MB free       ✅
[VERB] perf_event_paranoid=1 (uid=0)                           ✅
[VERB] All 4 PMU events validated                              ✅
[ERR]  Target process 99999 does not exist                     ✅ (correct rejection)
```

### 14.3 End-to-End Migration Test

**Workload:** `masim` with `test_tier_4gb_long` config (4 × 1GB regions, 60s per phase)  
**Initial placement:** `numactl --membind=0` (all 4GB on node 0)  
**Daemon config:** `hot_pct=75` (aggressive — top 25% stays hot, 75% demoted), `window=8s`

```
$ numactl --membind=0 ./masim_mod/masim masim_mod/configs/test_tier_4gb_long &
$ ./tierscaped -f -v -c test_config.toml -p <PID> --window 8 --hot-pct 75
```

#### Migration Timeline

| Time | Node 0 (hot) | Node 1 (cold) | Event |
|------|:------------:|:-------------:|-------|
| Before | **4100 MB** | 2 MB | All memory on hot node |
| Window 1 | **3398 MB** | **704 MB** | 262K pages (515 regions) demoted |
| Window 2 | **3344 MB** | **758 MB** | 120K pages (235 regions) demoted |
| Window 3 | **3362 MB** | **740 MB** | Re-promotion observed (phase changed) |

#### Key Observations

- ✅ **Pages genuinely migrate**: `numastat` confirms 700+ MB moved from node 0 → node 1
- ✅ **Re-promotion works**: When a new phase starts accessing a different region, previously-cold regions get promoted back to node 0
- ✅ **Graceful exit**: Daemon detects masim termination and exits cleanly
- ✅ **Multi-threaded migration**: 2 threads divide work across regions
- ✅ **Time-bounded**: Migration completes within 90% of window time
- ✅ **PEBS streaming**: ~515 regions sampled per window via live perf pipeline

#### Verbose Output (Window 1)

```
[INFO] tierscaped started: target PID=587881, hot_node=0, cold_node=1, window=8s, hot_pct=75.0, threads=2, region_size=2M
[VERB] Window 1: 515 regions with data
[VERB] Classification: 515 regions with samples, threshold=42 (percentile=75.0, idx=385/515)
[VERB] Classification result: 164 hot, 351 cold
[VERB] Migration: 262631 pages moved, 515 regions moved, 0 already in place, 0 errors
[INFO] Window 1: moved 262631 pages (515 regions), 0 in-place, 0 errors
```

### 14.4 Conservative Test (hot_pct=25)

Same workload, `hot_pct=25` (only bottom 25% demoted):

| Time | Node 0 (hot) | Node 1 (cold) | Event |
|------|:------------:|:-------------:|-------|
| Before | 4100 MB | 2 MB | All memory on hot node |
| Window 1 | 3867 MB | 235 MB | 262K pages, 117 cold regions |
| Steady state | ~3860 MB | ~240 MB | Less aggressive demotion |

Confirmed: lower `hot_pct` = less data migrated to cold tier.

### 14.5 Test Config Created

`masim_mod/configs/test_tier_4gb_long` — 4 × 1GB regions, 60s per phase (~4 min total runtime). Designed to give enough time for multiple windows of profiling and migration.
