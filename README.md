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



### Running Tiering with MASim

TierScape includes comprehensive MASIM experiments to evaluate different tiering strategies. Follow these steps:

#### Build and Test MASim
```bash
cd <root dir of repo>
make setup           # Sets up the environment and dependencies
make build_masim     # Builds the MASim simulator
make test_masim      # Tests MASim with sample configuration
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
make tier_masim_all
```

This will execute all four tiering strategies (baseline, hemem, ilp, waterfall) in sequence and generate comprehensive results.

### Running Tiering with Memcached

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



## Results and Evaluation

After running experiments, results are stored in the following locations:

- **Performance Data**: Results are stored in `evaluation/` directories
- **SKD Daemon Results**: Check `skd_daemon/evaluation/` for detailed performance metrics

## Understanding the Results

The experiments generate data comparing different tiering strategies:
- **Baseline (-1)**: No tiering, all data in single tier
- **HeMem (0)**: HeMem-based tiering algorithm
- **ILP (1)**: Integer Linear Programming-based optimal tiering
- **Waterfall (2)**: Waterfall-based tiering strategy

Key metrics include:
- Memory usage across tiers
- Access latency improvements
- Overall performance

> **Note:** Instructions for building and running with a custom Linux kernel will be added soon.



