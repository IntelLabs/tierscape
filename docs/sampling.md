# PEBS Sampling

## Pipeline

The sampler spawns one shell child running:

```bash
perf record -d -e <e1> -e <e2> ... -c <period> -p <PID> -o - 2>/dev/null \
  | perf script -i - --fields=time,addr 2>/dev/null
```

Key flags:

| Flag | Why |
|------|-----|
| `-d` | Record **data addresses** (memory locations accessed), not instruction pointers |
| `-e <event>:P` | PEBS precise sampling; `:Pu` = userspace only |
| `-c <period>` | Period-based sampling. **Required** for streaming — `-F <freq>` does *not* stream from `perf record -o -` on most kernels |
| `-p <PID>` | Attach to target process |
| `-o -` | Stream `perf.data` to stdout |
| `--fields=time,addr` | Compact output: `7938690.927119:     7ffe60e8b0b0` |

## Spawning model

Implemented in [src/sampler.cpp](../src/sampler.cpp):

1. `pipe(pipefd)`.
2. `fork()`.
3. Child: `setsid()` (new session, child PID == its PGID),
   `dup2(pipefd[1], STDOUT_FILENO)`, `execl("/bin/sh", "sh", "-c", cmd)`.
4. Parent: closes `pipefd[1]`, reads from `pipefd[0]` in the sampler thread.

Because the child is in its own process group, shutdown is one syscall:

```cpp
killpg(child_pgid, SIGTERM);
waitpid(child_pgid, ...);   // 50 ms timeout, then SIGKILL
```

This kills `sh`, `perf record`, and `perf script` together —
nothing is left dangling.

## Line parser

The parser reads up to 8 KiB at a time into a `std::string carry`
and splits on `\n`. For each line:

1. Find `:` (separates timestamp from address).
2. Skip whitespace; `strtoull(addr_str, nullptr, 16)`.
3. Filter:
   * `addr < 0x1000` — null-ish, garbage.
   * `addr >= 0x800000000000` — kernel space (x86_64 userspace cap).
4. Page-align: `addr &= ~(sysconf(_SC_PAGESIZE) - 1)`.
5. Deliver via callback to `RegionManager::add_sample(addr)`.

## Event configuration

Default events in [src/config.h](../src/config.h):

```cpp
"mem_inst_retired.all_loads:P",
"mem_inst_retired.all_stores:P",
```

Override via TOML:

```toml
[sampling]
events = [
    "mem_inst_retired.all_loads:P",
    "mem_inst_retired.all_stores:P",
    "mem_inst_retired.stlb_miss_loads:P",
    "mem_inst_retired.stlb_miss_stores:P",
]
frequency = 10000     # perf -c period
window_seconds = 20
```

> When `--dump-file` is set, every parsed sample is written as
> `"time_ms addr"`. `time_ms` is anchored to the first observed
> perf timestamp (atomic compare-exchange on the first sample), so
> the dump's timeline aligns with the daemon's per-window log even
> across the sampler/window thread boundary.

### Choosing events

* `mem_inst_retired.all_loads:P` / `all_stores:P` — most universal;
  covers cache hits and misses.
* `mem_load_uops_l3_miss_retired.local_dram:P` — only L3 misses;
  reduces sample rate by ~50× but loses warm-cache lines.
* `*.stlb_miss_loads:P` — TLB misses; useful for huge-page workloads.

Validate availability with `perf list`. The daemon also
auto-validates at startup — see [troubleshooting.md](troubleshooting.md).

### Choosing frequency (`-c period`)

| Period | Approx sample rate | Overhead |
|--------|---------------------|----------|
| 100 | very high | 150 %+ |
| 1 000 | high | ~30 % |
| **10 000** (default) | moderate | < 10 % |
| 100 000 | low | < 1 % |

Lower period = more samples, more migration accuracy, higher
overhead. Default `10 000` is a balanced starting point for most
workloads.

## Lifetimes

* Sampler is created once per daemon run.
* Sampler thread is `joined()` on shutdown after `Sampler::stop()`.
* No restart-on-failure logic: if `perf` crashes, the daemon logs
  EOF and the window loop continues with stale samples until the
  target dies or `SIGTERM`.
