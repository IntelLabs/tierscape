# TierScape: Two-Tier Memory Tiering Daemon

A lightweight userspace daemon that automatically tiers memory between two NUMA nodes (e.g., DRAM and CXL/Optane) using hardware performance counters (PEBS) for access profiling and `move_pages()` for migration.

## How It Works

```
tierscaped attaches to a running process (or launches one), profiles memory
access patterns via PEBS sampling, and periodically migrates pages between
a hot tier (fast NUMA node) and a cold tier (slow NUMA node) based on a
configurable percentile hotness threshold.
```

**Key features:**
- Single self-contained binary — no external scripts or solvers
- TOML config file with CLI overrides
- Percentile-based hot/cold classification
- Multi-threaded `move_pages()` migration
- Automatic sanity checks (NUMA nodes, perf, PMU events)
- Daemonizes by default; foreground mode for debugging

## Requirements

- Linux with 2+ NUMA nodes
- `perf` binary matching your kernel (with PEBS support)
- `libnuma-dev` (build dependency)
- `cmake >= 3.10`, `g++` with C++17 support
- Root access (for `move_pages()` and perf)

## Quick Start

### 1. Build

```bash
cd src
make
```

This produces `src/build/tierscaped`.

### 2. Configure

Copy and edit the example config:

```bash
cp tierscaped.toml.example tierscaped.toml
```

Edit `tierscaped.toml`:

```toml
[tiers]
hot_node = 0          # NUMA node for hot/fast tier
cold_node = 1         # NUMA node for cold/slow tier

[sampling]
# PEBS events (use `perf list` to find correct names for your CPU)
events = [
    "mem_inst_retired.all_loads:P",
    "mem_inst_retired.all_stores:P",
    "mem_inst_retired.stlb_miss_loads:P",
    "mem_inst_retired.stlb_miss_stores:P",
]
frequency = 10000       # sample period (perf -c flag)
window_seconds = 20     # profiling window duration

[classification]
hot_percentile = 25.0   # regions >= this percentile stay on hot node

[migration]
threads = 2
max_pages_per_window = 5000000
region_size = "2M"

[daemon]
pidfile = "/tmp/tierscaped.pid"
verbose = false
log_file = ""
perf_bin = "/usr/bin/perf"   # path to perf binary for your kernel
```

> **Note:** The `perf_bin` path must point to a perf binary that matches your running kernel. Use `perf --version` to verify.

### 3. Verify Your System

```bash
# Check NUMA topology
numactl --hardware

# Verify perf works
perf stat -e mem_inst_retired.all_loads -a -- sleep 0.1
```

### 4. Run

```bash
# Attach to an existing process
sudo src/build/tierscaped -c tierscaped.toml -p <PID>

# Or launch a process with tiering
sudo src/build/tierscaped -c tierscaped.toml -- ./my_workload --args

# Foreground + verbose (for debugging/testing)
sudo src/build/tierscaped -f -v -c tierscaped.toml -p <PID>
```

### 5. Stop

```bash
kill $(cat /tmp/tierscaped.pid)
```

## Testing with masim

`masim_mod/` contains a memory access simulator for validating tiering behavior.

### Build masim

```bash
cd masim_mod && make
```

### Run a Quick Test

```bash
# Terminal 1: Start masim (4GB, 4 regions, 60s per phase)
numactl --membind=0 ./masim_mod/masim masim_mod/configs/test_tier_4gb_long

# Terminal 2: Attach tierscaped
sudo src/build/tierscaped -f -v -c tierscaped.toml \
    -p $(pgrep -f "masim.*test_tier") --window 8 --hot-pct 75

# Terminal 3: Watch migration happen
watch -n 2 "numastat -p $(pgrep -f 'masim.*test_tier')"
```

**Expected result:** Memory moves from node 0 → node 1 as cold regions are demoted. The actively-accessed region stays on node 0.

### Example Output

```
[INFO] tierscaped started: PID=587881, hot_node=0, cold_node=1, window=8s
[VERB] Window 1: 515 regions with data
[VERB] Classification: threshold=42, 164 hot, 351 cold
[VERB] Migration: 262631 pages moved, 515 regions, 0 errors
[INFO] Window 1: moved 262631 pages (515 regions), 0 in-place, 0 errors
```

### Available masim Configs

| Config | Memory | Duration | Use |
|--------|--------|----------|-----|
| `test_tier_4gb_long` | 4 GB | ~4 min | Quick validation |
| `stairs_plot_1gb` | 4 GB | 20s | Smoke test |
| `stairs_100GB_10GB` | ~90 GB | 8s | Large-scale test |
| `stairs_1TB_100g` | ~900 GB | minutes | Stress test |

## CLI Reference

```
tierscaped [OPTIONS] -p <PID>
tierscaped [OPTIONS] -- <command> [args...]

OPTIONS:
  -p, --pid <PID>         Target process PID
  -c, --config <path>     TOML config file
  --hot-node <N>          NUMA node for hot tier
  --cold-node <N>         NUMA node for cold tier
  --hot-pct <float>       Hot percentile threshold (default: 25.0)
  --freq <int>            PEBS sampling period (default: 10000)
  --threads <int>         Migration threads (default: 2)
  --window <int>          Window size in seconds (default: 20)
  --region-size <sz>      Region size (default: 2M)
  --max-pages <int>       Max pages per window (default: 5000000)
  -v, --verbose           Verbose logging
  -f, --foreground        Don't daemonize
  --dry-run               Profile only, no migration
  --pidfile <path>        PID file (default: /tmp/tierscaped.pid)
  --perf <path>           Path to perf binary
  -h, --help              Show help
```

CLI arguments override config file values.

## Repository Structure

```
├── src/                       # Daemon source code
│   ├── main.cpp               # Entry point, CLI, daemonization
│   ├── config.cpp/.h          # TOML config parser
│   ├── sanity.cpp/.h          # Startup sanity checks
│   ├── sampler.cpp/.h         # PEBS pipeline (perf record | perf script)
│   ├── classifier.cpp/.h      # Percentile-based hot/cold classification
│   ├── migrator.cpp/.h        # Multi-threaded move_pages()
│   ├── region.cpp/.h          # Region management
│   ├── util.cpp/.h            # Logging, process utilities
│   ├── CMakeLists.txt         # Build system
│   └── Makefile               # Convenience wrapper
├── masim_mod/                 # Memory access simulator (test workload)
│   ├── masim.c                # Simulator source
│   ├── configs/               # Workload configs
│   └── Makefile
├── tierscaped.toml.example    # Example configuration
├── plan_two_tier_18_5_26.md   # Design document with test results
└── LICENSE
```

## License

See [LICENSE](LICENSE).
FAST_NODE: 0
SLOW_NODE: 1
Disabling the prefetching
kernel.zswap_print_stat = 1
[ 3904.686103] zswap: Looking for a zpool zsmalloc zstd 0
[ 3904.686104] zswap: It looks like we already have a pool. zsmalloc zstd 0
[ 3904.686104] zswap: zswap: Adding zpool Type zsmalloc Compressor zstd BS 0
[ 3904.686105] zswap: Total pools now 4
[ 3904.686117] zswap: Looking for a zpool zsmalloc lzo 0
[ 3904.686118] zswap: using existing pool zsmalloc lzo 0
[ 3904.686125] zswap: ..
                 Request for a new pool: pool and compressor is zsmalloc lzo backing store value is 0
[ 3904.686125] zswap: Looking for a zpool zsmalloc lzo 0
[ 3904.686126] zswap: It looks like we already have a pool. zsmalloc lzo 0
[ 3904.686126] zswap: zswap: Adding zpool Type zsmalloc Compressor lzo BS 0
[ 3904.686126] zswap: Total pools now 4
[ 3904.686745]
               ------------
               Total zswap pools 4
[ 3904.686747] zswap: Tier CData       pool        compressor  backing     Pages       isCPUComp   Faults
[ 3904.686749] zswap: 0    0           zsmalloc    lzo         0           0           true        0
[ 3904.686751] zswap: 1    0           zsmalloc    zstd        0           0           true        0
[ 3904.686752] zswap: 2    0           zsmalloc    zstd        1           0           true        0
[ 3904.686753] zswap: 3    0           zbud        zstd        0           0           true        0


```

## Executing Experiments with Kernel Patches

Rebuild TierScape with kernel patches enabled.
Ensure the configuration is done as in [Configuration](#configuration) section.
```bash
$ cd <root dir of repo>
$ make setup ENABLE_NTIER=1
$ make tier_masim_ilp agg_mode=2
```
Run MASIM or memcached experiments as described in the [Quick Start](#2-quick-start-without-kernel-patches) section.

The results will be saved in the dir witn suffix `_EN1` indicating kernel patches are enabled.

## 4. Understanding the Results
After running experiments, results are stored in the following locations:
- **Performance Data**: Results are stored in `evaluation/` directories

The experiments generate data comparing different tiering strategies:
- **Baseline (-1)**: No tiering, all data in single tier
- **HeMem (0)**: HeMem-based tiering algorithm
- **ILP (1)**: Integer Linear Programming-based optimal tiering
- **Waterfall (2)**: Waterfall-based tiering strategy

### Dir structure and figures

Example: ``perflog-ILP-F10000-HT.9-R0-PT2-W5-20250909-200453``
Breakdown of the dir name:
- `perflog`: Prefix indicating performance logs
- `ILP`: Tiering strategy used (Baseline, HeMem, ILP, Waterfall)
- `F10000`: PEBS frequency (10000)
- `HT.9`: Hotness threshold (0.9)
- `R0`: Remote mode (0 disabled 1 enabled)
- `PT2`: Number of push threads to move data around
- `W5`: Profile window in seconds
- `20250909-200453`: Timestamp of the experiment run

Exmplae: `perflog-WATERFALL-F10000-HT25-PT2-W5-20250909-195830`

Breakdown of the dir name:
- `perflog`: Prefix indicating performance logs
- `WATERFALL`: Tiering strategy used (Baseline, HeMem, ILP, Waterfall)
- `F10000`: PEBS frequency (10000)
- `HT25`: Hotness threshold (25 percentile)
- `PT2`: Number of push threads to move data around
- `W5`: Profile window in seconds
- `20250909-195830`: Timestamp of the experiment run

Similarly for hemem.

#### Figures
After runing each experiments, there will be plot directory created inside the experiment directory.

- `plot_numastat_configured_tiers.png`: NUMA distribution of memory usage over time
- `plot_psi.png`: Pressure Stall Information over time
- `plot_regions_curr_tier.png`: The current tier distribution of memory regions over time as seen by Tierscape
- `plot_regions_curr_tier_sorted.png`: The current tier distribution of memory regions over time as seen by Tierscape (sorted by hotness)
- `plot_regions_dst_tier.png`: The destination tier distribution of memory regions over time as seen by Tierscape -- may differ from current tier due to migration delays
- `plot_regions_dst_tier_sorted.png`: The destination tier distribution of memory regions over time as seen by Tierscape (sorted by hotness) -- may differ from current tier due to migration delays
- `plot_regions_hotness.png`: The hotness distribution of memory regions over time as seen reported by PEBS
- `plot_stacked_tco_sep.png`: Stacked TCO breakdown over time
- `plot_stacked_zswap_usage.png`: Stacked zswap usage breakdown over time
- `plot_zswap_all_metrics.png`: zswap faults, compressed size, original size, and pages over time
- `status_VmRSS.png`: Resident Set Size over time
- `vmstat_pgmigrate_success.png`: Successful page migrations over time


## Reproducing the results in the paper
TODO



