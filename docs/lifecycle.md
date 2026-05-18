# Lifecycle: Signals, Daemonization, Shutdown

## Startup sequence

```
1. parse CLI / load TOML
2. install signal handlers (SIGTERM, SIGINT, SIGCHLD, SIGPIPE)
3. if launch mode (-- cmd):
       fork+exec the target; wait for /proc/<pid>/maps
4. sanity_check_all(): NUMA, perf, PMU events, /proc access
5. if not --foreground: double-fork daemonize, redirect stdio
6. write pidfile (default /tmp/tierscaped.pid)
7. construct RegionManager
8. construct Sampler, spawn sampler thread
9. enter window loop
```

## Signal handling

Implemented with `sigaction(2)` (not `signal(2)`):

| Signal | Handler | Effect |
|--------|---------|--------|
| `SIGTERM` | `on_termish` | `g_running.store(false)` — window loop drains then exits |
| `SIGINT` | `on_termish` | same |
| `SIGCHLD` | `on_sigchld` | `g_child_exited.store(true)` — main loop also polls `is_process_running` every second |
| `SIGPIPE` | `SIG_IGN` | Prevents the daemon from dying if the perf pipeline closes asymmetrically |

All handlers are async-signal-safe: they do nothing except set
atomic flags. The window loop and shutdown path do the actual
work.

## Daemonization

`daemonize()` does the canonical double-fork:

```
fork() → parent exits
setsid() → become session leader
fork() → first child exits (drops controlling tty for good)
freopen("/dev/null", "r", stdin)
freopen("/dev/null", "w", stdout)
freopen(log_file or /dev/null, "w", stderr)
```

The pidfile is written **after** daemonization so it contains the
final daemon PID, not the launcher's.

In `--foreground` mode none of this runs; stdio is inherited from
the invoker. Use this for debugging.

## Window-loop termination

The main loop terminates when either:

* `g_running == false` (signal received), or
* `is_process_running(target_pid) == false` (target died).

It checks both at the top of each window and inside the
`sleep(1)`-based wait. Worst-case shutdown latency: ~1 second.

## Shutdown sequence

```
1. log_info("Shutting down...")
2. sampler.stop():
     - sets m_stop_requested
     - killpg(child_pgid, SIGTERM)
3. sampler_thread.join():
     - reader sees EOF (perf process group died)
     - reaps the shell child with waitpid (50 ms grace, then SIGKILL)
4. remove_pidfile()
5. if launch mode: waitpid(target_pid, WNOHANG) to reap if zombie
6. log_info("exited cleanly after N windows")
7. main returns 0
```

## What happens on crashes

* If the daemon segfaults / OOMs: the sampler child process group
  is **not** killed (no signal handler runs). The perf processes
  remain attached to the (now-gone) target and will exit on their
  own when the target exits or `kill(target_pid, ...)` happens.
* Pidfile is **not** cleaned up. Stale pidfiles are harmless —
  a future daemon will overwrite.

## Run multiple instances

Each daemon needs a distinct pidfile and target PID. Example:

```bash
sudo tierscaped -c /etc/tierscaped-app1.toml \
    --pidfile /run/tierscaped-app1.pid -p $APP1_PID &
sudo tierscaped -c /etc/tierscaped-app2.toml \
    --pidfile /run/tierscaped-app2.pid -p $APP2_PID &
```

Multiple sampler child groups, multiple migrator thread pools, no
shared state between them.
