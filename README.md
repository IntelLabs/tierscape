# TierScape: Artifact Evaluation - EuroSys 2026

This repository contains the artifact for the paper "TierScape: Harnessing Multiple Compressed Tiers to Tame Server Memory TCO" published at EuroSys 2026.

<!-- define a counter that gets incremented with use -->


## 1. Overview

TierScape can be evaluated in two configurations:
1. **Without kernel patches** - Multiple byte-addressable tiers (default kernel)
2. **With kernel patches** - Multiple byte-addressable tiers + compressible tiers

This artifact evaluation demonstrates both configurations, with kernel patches being required only if you want to enable compressible tiers. 
For basic functionality with multiple byte-addressable tiers, the default kernel is sufficient.

**Result reproducibility:** The system used in the paper has DRAM and Intel Optane memory tiers. To repdoduce the performance results, Optane memory is a must (as it is a much slower memory compared to DRAM).

The artifact default setting is designed for a system with atleast 2 NUMA nodes. It will use NUMA node 0 as the fast memory tier and NUMA node 1 as the slow memory tier. This can be changed (see TBD)
Verify your system's NUMA topology:
```bash
numactl -H
```
The byte-addressable tiers appear as different NUMA nodes.

## 2. Quick Start (Without Kernel Patches)

For initial testing and basic functionality, you can use the default kernel:

### 2.1. Running Tiering with MASIM

TierScape includes comprehensive MASIM experiments to evaluate different tiering strategies. Follow these steps:

#### Build and Test MASIM
```bash
cd <root dir of repo>
make setup           # Sets up the environment and dependencies
make build_masim     # Builds the MASIM simulator
make test_masim      # Tests MASIM with sample configuration
```

#### Run Individual MASIM Experiments
You can run specific tiering experiments:

```bash
# Baseline (no tiering)
make tier_masim_baseline

# HeMem tiering strategy
make tier_masim_hemem

# ILP-based tiering strategy
make tier_masim_ilp

# Waterfall tiering strategy
make tier_masim_waterfall
```

#### Run All MASIM Experiments
To run all experiments sequentially:
```bash
## This will execute all four tiering strategies (baseline, hemem, ilp, waterfall) in sequence.
make tier_masim_all
```

See [Understanging the results](#3-understanding-the-results) section to interpret the results.

### 2.2 Running Tiering with Memcached

TierScape also supports memcached workloads using memtier_benchmark for realistic evaluation.

#### Install and Setup Memcached
```bash
# Install memcached server
make install_memcached

# Install memtier_benchmark (if not already installed)
make install_memtier_benchmark
```

### Run Memcached Experiments

#### Prerequisites for Memcached Experiments
- The system needs sufficient memory for the 40GB dataset
- Make sure no other processes are using port 11211

1. **Start memcached and load data:**
```bash
make start_memcached    # Starts memcached server
make load_memcached     # Loads 40GB dataset with 4K objects
```

2. **Run individual memcached tiering experiments:**
```bash
# Baseline (no tiering)
make tier_memcached_memtier_baseline

# HeMem tiering strategy
make tier_memcached_memtier_hemem

# ILP-based tiering strategy
make tier_memcached_memtier_ilp

# Waterfall tiering strategy
make tier_memcached_memtier_waterfall
```

3. **Run all memcached experiments:**
```bash
make tier_memcached_memtier_all
```


## 5. Running with kernel patches

### Building and Installing the Custom Kernel


### Fetch the kernel and apply patches

```bash
$ cd <root dir of repo>
$ git clone https://github.com/torvalds/linux.git --branch v5.17 --depth 1
$ cd linux
$ git am ../linux_patch/0001-tierscape-eurosys26.patch
```


Verify the patches are applied:

```bash
$ git log

commit 8d955619e152eabd14acefa19c4c819c053cf96a (HEAD -> tierscape)
Author: Sandeep Kumar <sandeep4.kumar@intel.com>
Date:   Tue Sep 2 04:48:30 2025 +0530

    tierscape eurosys26

commit f443e374ae131c168a065ea1748feac6b2e76613 (grafted, tag: v5.17)
Author: Linus Torvalds <torvalds@linux-foundation.org>
Date:   Sun Mar 20 13:14:17 2022 -0700

    Linux 5.17

    Signed-off-by: Linus Torvalds <torvalds@linux-foundation.org>

```

### Build the kernel
```bash
$ cp tierscape_config .config
$ make -j $(nproc)
## Select default values for any new options
$ sudo make modules_install -j $(nproc)
$ sudo make install -j $(nproc)
```
### Reboot into the new kernel
```bash
sudo reboot
```
After reboot, verify the new kernel is active:
```bash
$ uname -r
5.17.0-ntier-noiaa-v1+
```

### Enabling Compressible Tiers

```bash
$ sudo bash skd_daemon/shell_scripts/setup_ntiers.sh
Using ZRAM
Removing zram
Setting up zram
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


## 4. Understanding the Results
After running experiments, results are stored in the following locations:
- **Performance Data**: Results are stored in `evaluation/` directories

The experiments generate data comparing different tiering strategies:
- **Baseline (-1)**: No tiering, all data in single tier
- **HeMem (0)**: HeMem-based tiering algorithm
- **ILP (1)**: Integer Linear Programming-based optimal tiering
- **Waterfall (2)**: Waterfall-based tiering strategy

> **Note:** Instructions for building and running with a custom Linux kernel will be added soon.



