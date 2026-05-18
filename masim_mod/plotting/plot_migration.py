#!/usr/bin/env python3
"""Plot NUMA migration and PEBS access pattern from eval experiment directory.

Produces two PNGs:
  - migration.png : memory distribution across NUMA nodes over time
  - access_pattern.png : scatter plot of sampled addresses over time

Usage:
    python3 plot_migration.py [--eval-dir PATH]

Defaults:
    --eval-dir  latest exp-* subdir under ../eval
"""

import argparse
import glob
import os
import re
import sys

import matplotlib.pyplot as plt
import numpy as np


def parse_numastat_log(filepath):
    """Parse numastat log -> (times[], node_data{nid: [MB]})."""
    times = []
    node_data = {}
    num_nodes = None

    with open(filepath) as f:
        current_time = None
        for line in f:
            m = re.match(r"=== T\+(\d+)s ===", line)
            if m:
                current_time = int(m.group(1))
                continue
            if current_time is not None and line.startswith("Total"):
                parts = line.split()
                values = [float(x) for x in parts[1:]]
                if num_nodes is None:
                    num_nodes = len(values) - 1
                    for nid in range(num_nodes):
                        node_data[nid] = []
                times.append(current_time)
                for nid in range(num_nodes):
                    node_data[nid].append(values[nid])
                current_time = None

    return times, node_data


def parse_sample_dump(filepath, regions=None, subsample=50000):
    """Parse samples.dump file (time_ms addr) -> (times_sec[], addrs[]).

    If regions are provided, filters to only addresses within those regions.
    Subsamples to keep plot manageable.
    """
    times = []
    addrs = []

    with open(filepath) as f:
        header = f.readline()  # skip header
        for line in f:
            parts = line.split()
            if len(parts) != 2:
                continue
            t_ms = int(parts[0])
            addr = int(parts[1], 16)
            times.append(t_ms / 1000.0)
            addrs.append(addr)

    times = np.array(times)
    addrs = np.array(addrs)

    # Filter to data regions if known
    if regions:
        region_min = min(r[1] for r in regions)
        region_max = max(r[2] for r in regions)
        mask = (addrs >= region_min) & (addrs <= region_max)
        if mask.sum() > 100:
            times = times[mask]
            addrs = addrs[mask]

    # Subsample if too many points
    if len(times) > subsample:
        idx = np.linspace(0, len(times) - 1, subsample, dtype=int)
        times = times[idx]
        addrs = addrs[idx]

    return times, addrs


def find_file(eval_dir, pattern):
    """Find file matching glob pattern in eval_dir."""
    files = sorted(glob.glob(os.path.join(eval_dir, pattern)))
    return files[-1] if files else None


def find_eval_dir(base):
    """If base contains exp-* subdirs, return the latest one; else return base."""
    exp_dirs = sorted(glob.glob(os.path.join(base, "exp-*")))
    if exp_dirs:
        return exp_dirs[-1]
    return base


def plot_migration(eval_dir, output):
    """Generate migration.png."""
    logfile = find_file(eval_dir, "numastat*.log")
    if not logfile:
        print(f"Error: no numastat log in {eval_dir}", file=sys.stderr)
        return False

    print(f"Parsing: {logfile}")
    times, node_data = parse_numastat_log(logfile)
    if not times:
        print("Error: no data points parsed", file=sys.stderr)
        return False

    active_nodes = [nid for nid, vals in node_data.items() if max(vals) > 0.1]
    active_nodes.sort()

    colors = plt.cm.tab10.colors
    fig, ax = plt.subplots(figsize=(10, 5))

    for nid in active_nodes:
        color = colors[nid % len(colors)]
        ax.plot(times, node_data[nid], marker='o', markersize=4,
                linewidth=2, label=f"Node {nid}", color=color)

    ax.set_xlabel("Time (s)", fontsize=12)
    ax.set_ylabel("Memory (MB)", fontsize=12)
    ax.set_title("Data Migration Between NUMA Nodes", fontsize=14)
    ax.legend(fontsize=11)
    ax.grid(True, alpha=0.3)
    ax.set_xlim(left=0)
    ax.set_ylim(bottom=0)

    plt.tight_layout()
    plt.savefig(output, dpi=150)
    plt.close()
    print(f"Saved: {output}")
    return True


def parse_masim_regions(eval_dir):
    """Parse masim.log for region addresses. Returns list of (name, start, end)."""
    logfile = find_file(eval_dir, "masim*.log")
    if not logfile:
        return []
    regions = []
    with open(logfile) as f:
        for line in f:
            m = re.match(r"Region (\d+) address is (0x[0-9a-f]+) : (0x[0-9a-f]+)", line)
            if m:
                idx = int(m.group(1))
                start = int(m.group(2), 16)
                end = int(m.group(3), 16)
                regions.append((f"s{idx}", start, end))
    return regions


def plot_access_pattern(dump_path, output, eval_dir=None):
    """Generate access_pattern.png from PEBS sample dump."""
    # Get region info for filtering and labeling
    regions = []
    if eval_dir:
        regions = parse_masim_regions(eval_dir)

    print(f"Parsing: {dump_path}")
    times, addrs = parse_sample_dump(dump_path, regions=regions if regions else None)

    if len(times) == 0:
        print("Error: no samples in dump file", file=sys.stderr)
        return False

    # Convert to GB offset from base of data regions
    addr_min = addrs.min()
    addr_max = addrs.max()
    addrs_gb = (addrs - addr_min) / (1024**3)

    fig, ax = plt.subplots(figsize=(10, 5))
    ax.scatter(times, addrs_gb, s=0.3, alpha=0.4, c='steelblue', edgecolors='none')

    # Draw region boundaries if available
    if regions:
        colors = plt.cm.Set1.colors
        for i, (name, rstart, rend) in enumerate(regions):
            if rstart >= addr_min:
                y_lo = (rstart - addr_min) / (1024**3)
                y_hi = (rend - addr_min) / (1024**3)
                ax.axhspan(y_lo, y_hi, alpha=0.08, color=colors[i % len(colors)])
                ax.text(times.max() * 1.01, (y_lo + y_hi) / 2, name,
                        fontsize=9, va='center', color=colors[i % len(colors)],
                        fontweight='bold')

    ax.set_xlabel("Time (s)", fontsize=12)
    ax.set_ylabel("Address offset (GB)", fontsize=12)
    ax.set_title("PEBS Sampled Memory Access Pattern", fontsize=14)
    ax.grid(True, alpha=0.3)
    ax.set_xlim(left=0)
    # Y-axis limits from actual sample boundaries
    y_max = (addr_max - addr_min) / (1024**3)
    ax.set_ylim(bottom=0, top=y_max * 1.05)

    plt.tight_layout()
    plt.savefig(output, dpi=150)
    plt.close()
    print(f"Saved: {output}")
    return True


def main():
    parser = argparse.ArgumentParser(description="Plot migration and access pattern")
    parser.add_argument("--eval-dir", default=None,
                        help="Experiment directory (default: latest exp-* in ../eval)")
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    base_eval = os.path.join(script_dir, "..", "eval")

    if args.eval_dir:
        eval_dir = os.path.abspath(args.eval_dir)
    else:
        eval_dir = find_eval_dir(base_eval)

    print(f"Using eval dir: {eval_dir}")

    # Plot 1: Migration
    mig_out = os.path.join(eval_dir, "migration.png")
    plot_migration(eval_dir, mig_out)

    # Plot 2: Access pattern from PEBS dump
    dump_file = find_file(eval_dir, "samples.dump")
    if dump_file:
        access_out = os.path.join(eval_dir, "access_pattern.png")
        plot_access_pattern(dump_file, access_out, eval_dir=eval_dir)
    else:
        print("Warning: no samples.dump found, skipping access pattern plot",
              file=sys.stderr)


if __name__ == "__main__":
    main()
