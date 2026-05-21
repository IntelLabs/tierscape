#pragma once

#include "config.h"

// Load the full effective configuration from CLI args and (optionally)
// a TOML file: TOML defaults first, CLI flags override. Handles
//   -c FILE / --config=FILE
//   -p PID  / -- <cmd ...>
//   --hot-node / --cold-node / --hot-pct / --freq / --threads
//   --window / --region-size / --max-pages / --max-idle
//   --dump-file / --pidfile / --perf / -v / -f / --dry-run / -h
//
// Returns 0 on success, -1 on a parse error. Prints usage on -h and
// exits the process directly.
int cli_load(int argc, char** argv, Config& cfg);
