# Troubleshooting

## Sanity-check failures at startup

| Error | Meaning | Fix |
|-------|---------|-----|
| `NUMA hot node N does not exist` | `/sys/devices/system/node/nodeN/` missing | Check `numactl -H`. Pick an existing node ID |
| `NUMA hot/cold node N has zero memory (size64=...)` | Node is CPU-only or hot-unplugged | Choose a node with memory |
| `hot_node (X) and cold_node (X) must be different` | Same node configured for both tiers | Edit TOML or CLI |
| `perf binary not found or not executable` | `perf_bin` path wrong | Verify `which perf` and the kernel version it ships with |
| `perf_event_paranoid=N and not running as root` | Kernel restricts perf | Either run as root or `echo 1 > /proc/sys/kernel/perf_event_paranoid` |
| `PMU event validation failed (rc=N)` | One of the events in `[sampling].events` isn't supported on this CPU | Use `perf list` to find valid event names |
| `Target process N does not exist` | Bad `--pid` | Check `ps -p N` |
| `Cannot read /proc/<pid>/pagemap (need root or CAP_SYS_PTRACE)` | Permission issue | Run as root |

## Runtime symptoms

### "no samples" every window

```
Window N: no samples
```

Possible causes:

* Workload isn't accessing memory yet (warming up). Wait a few windows.
* PEBS events don't fire on this CPU. Check `perf stat -e
  <event> -p <pid> -- sleep 5` manually.
* perf pipeline died — check
  `cat /proc/<sampler_child_pid>/status`. The daemon currently
  does **not** restart a dead pipeline.

### Zero `pages_moved` over many windows

```
Window N: moved 0 pages (M regions), L in-place, 0 errors, 0 skipped
```

Most common cause: the workload is already split across both tiers,
so the classifier's target = current and no movement is needed.
Verify with `numastat -p <pid>`.

If `in_place == 0` and `pages_moved == 0` and `skipped_no_vma == 0`:
classifier may be returning the wrong target for every region.
Check `hot_node`/`cold_node` settings.

### `skipped_no_vma` high

The classifier sees PEBS samples whose addresses don't fall in any
**anonymous writable** VMA. This happens when:

* Workload primarily reads file-backed data (mmap'd files).
* Library code dominates the load events.

Mitigations:

* Add `:Pu` suffix to events for userspace-only counting.
* Switch to events that exclude code fetches (e.g.,
  `mem_load_uops_retired.l3_miss:P`).
* Accept it — those pages genuinely shouldn't be moved.

### `errors` non-zero

```
Window N: moved X pages (Y regions), 0 in-place, Z errors, 0 skipped
```

If `errors > 0`:

* **Target NUMA node is full** — `move_pages` returns `-ENOMEM` per
  page. Check `numastat -m | head -20` for free memory on the
  target. Reduce `max_pages_per_window`, raise `hot_percentile`
  (demote more), or pick a different node.
* **Permissions** — daemon not running as root, or target's
  pagemap revoked mid-run. Re-launch as root.
* **Target died mid-migration** — race; the next iteration of the
  window loop will detect and shut down.

### Daemon won't shut down

```
$ kill $(cat /tmp/tierscaped.pid)
# no exit after several seconds
```

* Daemon checks `g_running` and target liveness **once per
  second** during the inter-window sleep, so a graceful stop takes
  up to 1 second when the daemon is sleeping. If the signal arrives
  mid-window (i.e. during snapshot/classify/migrate) it is observed
  only when control returns to the sleep loop — worst case up to
  one full `window_seconds` for very large migrations.
* If still stuck, the sampler thread may be blocked in `read()` on
  a hung perf process. `kill -9` the daemon PID, then
  `pkill perf` to clean up the pipeline.
* This shouldn't happen with the current implementation
  (`killpg(child_pgid, SIGTERM)` + 50 ms SIGKILL escalation in
  `Sampler::shutdown_child`). If it does, file a bug.

### Stale pidfile

`/tmp/tierscaped.pid` is **not** removed if the daemon crashes.
A subsequent run will overwrite it — no error. But other tools
that read the pidfile may see a stale PID. Either:

* `rm /tmp/tierscaped.pid` manually before start.
* Pick a different `--pidfile` per daemon instance.

## Building

| Error | Fix |
|-------|-----|
| `fatal error: numa.h: No such file or directory` | `apt install libnuma-dev` (or distro equivalent) |
| `c++: error: unrecognized command-line option '-std=c++17'` | Need GCC ≥ 7 / Clang ≥ 5 |
| CMake says version < 3.10 | Use `make` wrapper instead, or upgrade cmake |

## Diagnostic dumps

To capture everything in one place:

```bash
# Daemon state
ps -ef | grep tierscaped
cat /tmp/tierscaped.pid
ls -l /proc/$(cat /tmp/tierscaped.pid)/task/

# Target state
numastat -p <pid>
cat /proc/<pid>/numa_maps | head -30

# Perf health
perf list | grep mem_inst
cat /proc/sys/kernel/perf_event_paranoid

# Kernel
uname -r
numactl -H
```
