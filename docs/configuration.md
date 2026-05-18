# Configuration

## Precedence

CLI flags > TOML config file > built-in defaults.

The TOML file is searched, in order:

1. Path passed via `-c <path>` / `--config <path>`.
2. `./tierscaped.toml` in the current directory.
3. `/etc/tierscaped.toml`.

Unknown keys in TOML trigger a `[WARN]` log line and are ignored.

## All config keys

### `[tiers]`

| Key | Default | CLI override | Description |
|-----|---------|--------------|-------------|
| `hot_node` | `0` | `--hot-node N` | NUMA node ID for hot tier |
| `cold_node` | `1` | `--cold-node N` | NUMA node ID for cold tier |

Both must exist (per `/sys/devices/system/node/nodeN`) and have
non-zero memory. They cannot be equal.

### `[sampling]`

| Key | Default | CLI override | Description |
|-----|---------|--------------|-------------|
| `events` | `["mem_inst_retired.all_loads:P", "mem_inst_retired.all_stores:P"]` | — | List of PEBS event strings passed to `perf record -e` |
| `frequency` | `10000` | `--freq N` | Period in `perf -c <period>` — sample every N events |
| `window_seconds` | `20` | `--window N` | How long each profiling/migration cycle lasts |

### `[classification]`

| Key | Default | CLI override | Description |
|-----|---------|--------------|-------------|
| `hot_percentile` | `25.0` | `--hot-pct F` | 0–100. Regions whose hotness sits **below** this percentile are demoted to `cold_node`. See [classification.md](classification.md) |

### `[migration]`

| Key | Default | CLI override | Description |
|-----|---------|--------------|-------------|
| `threads` | `2` | `--threads N` | Parallel `move_pages` workers |
| `max_pages_per_window` | `5000000` | `--max-pages N` | Hard cap on pages migrated in one window (≈ 20 GiB @ 4 KiB) |
| `region_size` | `"2M"` | `--region-size SZ` | Virtual-address bin size. `K`/`M`/`G` suffixes accepted |
| `max_idle_windows` | `10` | `--max-idle N` | Evict regions silent for this many consecutive windows |

### `[daemon]`

| Key | Default | CLI override | Description |
|-----|---------|--------------|-------------|
| `pidfile` | `"/tmp/tierscaped.pid"` | `--pidfile PATH` | Where to write the daemon PID after daemonization |
| `verbose` | `false` | `-v` / `--verbose` | Emit `[VERB]` log lines |
| `log_file` | `""` | — | If non-empty (and not foreground), redirect stderr here on daemonize |
| `perf_bin` | `"/usr/bin/perf"` | `--perf PATH` | Path to the `perf` binary. **Must match the running kernel version** |

## CLI-only flags

| Flag | Description |
|------|-------------|
| `-p, --pid <PID>` | Target PID (attach mode) |
| `--` | Everything after is the command line to launch (launch mode) |
| `-c, --config <path>` | TOML config path |
| `-f, --foreground` | Don't daemonize. Stdio inherited |
| `--dry-run` | Profile only, never call `move_pages` |
| `-h, --help` | Print usage and exit |

## Example TOML

See [tierscaped.toml.example](../tierscaped.toml.example) at the
repo root. A minimal working file:

```toml
[tiers]
hot_node  = 0
cold_node = 1

[sampling]
events         = ["mem_inst_retired.all_loads:P", "mem_inst_retired.all_stores:P"]
frequency      = 10000
window_seconds = 10

[classification]
hot_percentile = 25.0

[migration]
threads              = 2
max_pages_per_window = 5000000
region_size          = "2M"
max_idle_windows     = 10

[daemon]
pidfile  = "/tmp/tierscaped.pid"
verbose  = true
perf_bin = "/usr/bin/perf"
```

## Validation

All numeric CLI inputs are parsed with `strtol`/`strtod` and range-checked:

| Field | Accepted range |
|-------|----------------|
| `--pid` | `1..INT32_MAX` |
| `--hot-node`, `--cold-node` | `0..1023` |
| `--hot-pct` | `0.0..100.0` |
| `--freq` | `1..INT32_MAX` |
| `--threads` | `1..1024` |
| `--window`, `--max-idle` | `1..86400` and `1..INT32_MAX` respectively |
| `--max-pages` | `1..INT64_MAX` |
| `--region-size` | Non-zero after suffix parsing |

Invalid values produce a `[ERR]` log line and exit code 1.
