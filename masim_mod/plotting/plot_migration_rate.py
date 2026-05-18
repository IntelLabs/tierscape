#!/usr/bin/env python3
"""Plot tierscaped migration rate per window from tierscaped.log.

Generates: migration_rate.png

Shows pages moved (demotion) and classification breakdown per window.
"""

import argparse
import glob
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import acm_style

import matplotlib.pyplot as plt


def parse_tierscaped_log(filepath):
    """Parse tierscaped.log for per-window migration and classification stats.

    Returns dict with lists keyed by:
        window, pages_moved, pages_promoted, pages_demoted, regions_moved, hot, cold
    """
    data = {'window': [], 'pages_moved': [], 'pages_promoted': [],
            'pages_demoted': [], 'regions_moved': [],
            'hot': [], 'cold': [], 'in_place': [], 'errors': []}

    with open(filepath) as f:
        current_hot = None
        current_cold = None
        for line in f:
            # Classify result: 155 hot, 359 cold
            m = re.search(r'Classify result: (\d+) hot, (\d+) cold', line)
            if m:
                current_hot = int(m.group(1))
                current_cold = int(m.group(2))
                continue

            # New format: Window N: moved P pages (X promoted, Y demoted), R regions, ...
            m = re.search(
                r'Window (\d+): moved (\d+) pages \((\d+) promoted, (\d+) demoted\), '
                r'(\d+) regions, (\d+) in-place, (\d+) errors', line)
            if m:
                w = int(m.group(1))
                data['window'].append(w)
                data['pages_moved'].append(int(m.group(2)))
                data['pages_promoted'].append(int(m.group(3)))
                data['pages_demoted'].append(int(m.group(4)))
                data['regions_moved'].append(int(m.group(5)))
                data['in_place'].append(int(m.group(6)))
                data['errors'].append(int(m.group(7)))
                data['hot'].append(current_hot if current_hot is not None else 0)
                data['cold'].append(current_cold if current_cold is not None else 0)
                current_hot = None
                current_cold = None
                continue

            # Legacy format: Window N: moved P pages (R regions), I in-place, E errors, ...
            m = re.search(
                r'Window (\d+): moved (\d+) pages \((\d+) regions\), '
                r'(\d+) in-place, (\d+) errors', line)
            if m:
                w = int(m.group(1))
                data['window'].append(w)
                data['pages_moved'].append(int(m.group(2)))
                data['pages_promoted'].append(0)
                data['pages_demoted'].append(int(m.group(2)))
                data['regions_moved'].append(int(m.group(3)))
                data['in_place'].append(int(m.group(4)))
                data['errors'].append(int(m.group(5)))
                data['hot'].append(current_hot if current_hot is not None else 0)
                data['cold'].append(current_cold if current_cold is not None else 0)
                current_hot = None
                current_cold = None

    return data


def find_file(eval_dir, pattern):
    files = sorted(glob.glob(os.path.join(eval_dir, pattern)))
    return files[-1] if files else None


def find_eval_dir(base):
    exp_dirs = sorted(glob.glob(os.path.join(base, "exp-*")))
    if exp_dirs:
        return exp_dirs[-1]
    return base


def main():
    parser = argparse.ArgumentParser(description="Plot migration rate per window")
    parser.add_argument("--eval-dir", default=None)
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    base_eval = os.path.join(script_dir, "..", "eval")

    if args.eval_dir:
        eval_dir = os.path.abspath(args.eval_dir)
    else:
        eval_dir = find_eval_dir(base_eval)

    logfile = find_file(eval_dir, "tierscaped*.log")
    if not logfile:
        print(f"Error: no tierscaped log in {eval_dir}", file=sys.stderr)
        sys.exit(1)

    print(f"Parsing: {logfile}")
    data = parse_tierscaped_log(logfile)

    if not data['window']:
        print("Error: no window data parsed", file=sys.stderr)
        sys.exit(1)

    acm_style.apply()

    # Convert pages to MB (4K pages)
    promoted_mb = [p * 4096 / (1024**2) for p in data['pages_promoted']]
    demoted_mb = [p * 4096 / (1024**2) for p in data['pages_demoted']]
    windows = data['window']
    # Time axis: each window is 10s
    window_sec = [w * 10 for w in windows]

    fig, (ax1, ax2) = acm_style.fig_subplots(nrows=2, ncols=1, h=4.0, double=False)

    # Top panel: promoted and demoted MB per window (stacked)
    bar_w = 8
    ax1.bar(window_sec, promoted_mb, width=bar_w, color=acm_style.COLORS[1],
            alpha=0.85, edgecolor='black', linewidth=0.4, label='Promoted (→ hot)')
    ax1.bar(window_sec, demoted_mb, width=bar_w, bottom=promoted_mb,
            color=acm_style.COLORS[3], alpha=0.85, edgecolor='black',
            linewidth=0.4, label='Demoted (→ cold)')
    acm_style.finalize(ax1, ylabel='Data moved (MB)',
                       title='Migration Rate per Window',
                       legend_loc='upper right', grid_on=True)
    ax1.set_xlim(left=0, right=max(window_sec) + 10)

    # Bottom panel: classification breakdown (hot vs cold regions)
    ax2.bar(window_sec, data['hot'], width=bar_w, color=acm_style.COLORS[1],
            alpha=0.85, edgecolor='black', linewidth=0.4, label='Hot (promote)')
    ax2.bar(window_sec, data['cold'], width=bar_w, bottom=data['hot'],
            color=acm_style.COLORS[3], alpha=0.85, edgecolor='black',
            linewidth=0.4, label='Cold (demote)')
    acm_style.finalize(ax2, xlabel='Time (s)', ylabel='Regions',
                       title='Classification per Window',
                       legend_loc='upper right', grid_on=True)
    ax2.set_xlim(left=0, right=max(window_sec) + 10)

    acm_style.tight(fig)
    output = os.path.join(eval_dir, "migration_rate.png")
    fig.savefig(output, **acm_style.SAVEFIG)
    plt.close()
    print(f"Saved: {output}")


if __name__ == "__main__":
    main()
