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

            # New format: Window N: moved P pages (X promoted, Y demoted), R[/T] regions, ...
            m = re.search(
                r'Window (\d+): moved (\d+) pages \((\d+) promoted, (\d+) demoted\), '
                r'(\d+)(?:/\d+)? regions, (\d+) in-place, (\d+) errors', line)
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


def parse_masim_phases(filepath):
    """Parse a masim config and return cumulative phase boundary times (seconds).

    Each `phase N` block is followed by a numeric line giving its duration in ms.
    Returns a list of boundary times in seconds (exclusive of t=0, inclusive of total).
    Returns [] if file is missing or unparseable.
    """
    if not filepath or not os.path.isfile(filepath):
        return []
    boundaries = []
    cum_ms = 0
    expect_duration = False
    try:
        with open(filepath) as f:
            for line in f:
                s = line.strip()
                if not s or s.startswith('#'):
                    continue
                if s.lower().startswith('phase '):
                    expect_duration = True
                    continue
                if expect_duration:
                    # First non-comment line after `phase N` is the duration (ms).
                    try:
                        cum_ms += int(s.split()[0].rstrip(','))
                        boundaries.append(cum_ms / 1000.0)
                    except ValueError:
                        pass
                    expect_duration = False
    except OSError:
        return []
    # Drop the final boundary (= end of run), keep only intra-run transitions
    return boundaries[:-1] if len(boundaries) > 1 else []


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

    # Optional phase boundaries from masim config
    phases = parse_masim_phases(os.path.join(eval_dir, "masim_config"))
    if phases:
        print(f"Phase boundaries (s): {phases}")

    acm_style.apply()

    # Convert pages to MB (4K pages)
    promoted_mb = [p * 4096 / (1024**2) for p in data['pages_promoted']]
    demoted_mb = [p * 4096 / (1024**2) for p in data['pages_demoted']]
    windows = data['window']
    # Time axis: each window is 10s
    window_sec = [w * 10 for w in windows]

    fig, (ax1, ax2) = acm_style.fig_subplots(nrows=2, ncols=1, h=4.0, double=False)

    # Slightly smaller fonts for titles / axis labels
    LBL_FS = 7.5
    TTL_FS = 8.0

    def _draw_phases(ax):
        for t in phases:
            ax.axvline(t, color='0.4', linestyle='--', linewidth=0.6,
                       alpha=0.8, zorder=0)

    # Top panel: promoted and demoted MB per window (stacked)
    bar_w = 8
    ax1.bar(window_sec, promoted_mb, width=bar_w, color=acm_style.COLORS[1],
            alpha=0.85, edgecolor='black', linewidth=0.4, label='Promoted (→ hot)')
    ax1.bar(window_sec, demoted_mb, width=bar_w, bottom=promoted_mb,
            color=acm_style.COLORS[3], alpha=0.85, edgecolor='black',
            linewidth=0.4, label='Demoted (→ cold)')
    acm_style.finalize(ax1, legend_loc='upper right', grid_on=True)
    ax1.set_ylabel('Data moved (MB)', fontsize=LBL_FS)
    ax1.set_xlim(left=0, right=max(window_sec) + 10)
    _draw_phases(ax1)
    ax1.legend(loc='lower center', bbox_to_anchor=(0.5, 1.02),
               ncol=2, frameon=False, borderaxespad=0, fontsize=LBL_FS)
    ax1.set_title('(a) Migration Rate per Window', y=-0.28, fontsize=TTL_FS)
    y1_max = max([p + d for p, d in zip(promoted_mb, demoted_mb)] or [1])
    ax1.set_ylim(top=y1_max * 1.10)

    # Bottom panel: classification breakdown (hot vs cold regions)
    ax2.bar(window_sec, data['hot'], width=bar_w, color=acm_style.COLORS[1],
            alpha=0.85, edgecolor='black', linewidth=0.4, label='Hot (promote)')
    ax2.bar(window_sec, data['cold'], width=bar_w, bottom=data['hot'],
            color=acm_style.COLORS[3], alpha=0.85, edgecolor='black',
            linewidth=0.4, label='Cold (demote)')
    acm_style.finalize(ax2, legend_loc='upper right', grid_on=True)
    ax2.set_xlabel('Time (s)', fontsize=LBL_FS)
    ax2.set_ylabel('Regions', fontsize=LBL_FS)
    ax2.set_xlim(left=0, right=max(window_sec) + 10)
    _draw_phases(ax2)
    ax2.legend(loc='lower center', bbox_to_anchor=(0.5, 1.02),
               ncol=2, frameon=False, borderaxespad=0, fontsize=LBL_FS)
    ax2.set_title('(b) Classification per Window', y=-0.45, fontsize=TTL_FS)
    y2_max = max([h + c for h, c in zip(data['hot'], data['cold'])] or [1])
    ax2.set_ylim(top=y2_max * 1.10)

    acm_style.tight(fig)
    output = os.path.join(eval_dir, "migration_rate.png")
    fig.savefig(output, **acm_style.SAVEFIG)
    plt.close()
    print(f"Saved: {output}")


if __name__ == "__main__":
    main()
