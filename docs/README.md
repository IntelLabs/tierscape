# TierScape Documentation

This directory contains the architecture, design, and operational
documentation for `tierscaped`, the two-tier byte-addressable memory
tiering daemon.

## Contents

| Document | Description |
|----------|-------------|
| [architecture.md](architecture.md) | High-level architecture and component diagram |
| [design.md](design.md) | Detailed design rationale and algorithms |
| [region-management.md](region-management.md) | Region creation, address-space tracking, eviction |
| [sampling.md](sampling.md) | PEBS pipeline, event configuration, parsing |
| [migration.md](migration.md) | `move_pages` migration, VMA filtering, page-cap and time-cap budgets |
| [classification.md](classification.md) | Percentile-based hot/cold classification |
| [lifecycle.md](lifecycle.md) | Process model: signals, daemonization, shutdown |
| [configuration.md](configuration.md) | All config keys, defaults, CLI overrides |
| [testing.md](testing.md) | Test workflow with `masim`, log layout, validation |
| [troubleshooting.md](troubleshooting.md) | Common failure modes and remediation |
