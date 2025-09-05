# TierScape: Artifact Evaluation - EuroSys 2026

This repository contains the artifact for the paper "TierScape: Harnessing Multiple Compressed Tiers to Tame Server Memory TCO" published at EuroSys 2026.

## Overview

TierScape can be evaluated in two configurations:
1. **Without kernel patches** - Multiple byte-addressable tiers (default kernel)
2. **With kernel patches** - Multiple byte-addressable tiers + compressible tiers

This artifact evaluation demonstrates both configurations, with kernel patches being required only if you want to enable compressible tiers. For basic functionality with multiple byte-addressable tiers, the default kernel is sufficient.

## Quick Start (Without Kernel Patches)

For initial testing and basic functionality, you can use the default kernel:


### Check NUMA Configuration
Verify your system's NUMA topology:
```bash
numactl -H
```
The byte-addressable tiers appear as different NUMA nodes.


### Setup Environment
```bash
sudo bash setup_tierscape.sh
```

This script will:
- Configure the SKD daemon
- Run sanity checks
- Generate performance monitoring scripts
- Build the SKD daemon components

### Verify Installation with MASim

We use MASim (Memory Access Simulator) to test the system functionality:

```bash
cd masim_mod
make
./masim configs/stairs_1TB_100g
```

This will run a memory access pattern that allocates ~900GB and sequentially reads 400GB in a loop, generating approximately 20 billion memory accesses for testing.






<!-- 
## Full Setup (With Kernel Patches)

For complete TierScape functionality including compressible tiers:

### 1. Apply Kernel Patch

```bash
cd linux_patch
# Follow instructions in linux_patch/README.md to:
# - Clone Linux v5.17
# - Apply the TierScape patch
# - Build and install the patched kernel
# - Reboot into the new kernel
```

### 2. Setup Environment

After booting into the patched kernel:
```bash
sudo bash setup_tierscape.sh
```

### 3. Configure TierScape

The system supports 2 compressed tiers and 2 byte-addressable tiers. Configuration files are located in:
- `skd_daemon/skd_configs/` - SKD daemon configurations
- `masim_mod/configs/` - Test workload configurations

## Testing and Evaluation

### Available Test Configurations

The `masim_mod/configs/` directory contains various test configurations:
- `stairs_1TB_100g` - 1TB working set with 100GB active region
- `stairs_5TB_100g` - 5TB working set with 100GB active region  
- `needle_5TB_4k` - Needle-in-haystack pattern with 4KB accesses
- `zigzag_plot_1gb.cfg` - ZigZag access pattern

### Running Tests

1. **Basic functionality test:**
   ```bash
   cd masim_mod
   ./masim configs/stairs_1TB_100g
   ```

2. **Performance profiling:**
   ```bash
   cd perf_scripts
   bash run_profile.sh
   ```

3. **SKD daemon testing:**
   ```bash
   cd skd_daemon
   bash run_all.sh
   ```

## System Requirements

- Linux system with NUMA support
- Root access for setup
- Sufficient memory for large working sets (tests use up to 5TB)
- For kernel patches: ability to compile and install custom kernel

## Artifact Structure

- `masim_mod/` - Memory access simulator for testing
- `skd_daemon/` - SKD (Storage-class memory Kernel Daemon) implementation
- `ilp_server/` - ILP server for optimization decisions
- `linux_patch/` - Kernel patches for compressible tier support
- `perf_scripts/` - Performance monitoring and profiling tools

## Expected Results

After successful setup, you should be able to:
1. Run MASim workloads without errors
2. Observe memory tier migration through system logs
3. Generate performance profiles showing tier utilization
4. Demonstrate TCO improvements through the provided benchmarks -->


