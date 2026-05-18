# masim — Memory Access Simulator

A configurable memory access simulator used for testing and validating `tierscaped`.

## Build

```bash
make
```

## Run

```bash
./masim configs/<config file>
```

## Config File Format

Each config file defines memory regions and access patterns:

```
<total_bytes> <region_size_bytes>
<num_phases>
<phase_duration_seconds> <active_region_start> <active_region_end>
...
```

## Available Configs

| Config | Total Memory | Regions | Duration | Purpose |
|--------|-------------|---------|----------|---------|
| `test_tier_4gb_long` | 4 GB | 4 × 1 GB | ~4 min | Quick `tierscaped` validation |
| `stairs_plot_1gb` | 4 GB | 4 × 1 GB | ~20 s | Smoke test |
| `stairs_100GB_10GB` | ~90 GB | 9 × 10 GB | ~8 s | Large-scale test |
| `stairs_1TB_100g` | ~900 GB | 9 × 100 GB | minutes | Stress test |

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

