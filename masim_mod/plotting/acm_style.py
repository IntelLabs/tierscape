"""
acm_style.py
────────────────────────────────────────────────────────────────────────────────
ACM SigConf / USENIX-compatible matplotlib style helpers.

Usage
-----
    from acm_style import apply, MARKERS, LINES, COLORS, fig_single, fig_double

    apply()                          # call once at top of script

    fig, ax = fig_single()           # 3.33 × 2.6 in  (single column)
    fig, ax = fig_double()           # 7.00 × 2.6 in  (double column)
    fig, ax = fig_single(h=3.0)      # custom height

    # Iterate styles for multi-series plots
    for (label, data), style in zip(series.items(), acm_style.STYLES):
        ax.errorbar(x, data['mean'], yerr=data['std'],
                    label=label, **style)

    fig.savefig('fig.pdf', **acm_style.SAVEFIG)   # consistent save kwargs
    fig.savefig('fig.png', **acm_style.SAVEFIG)
────────────────────────────────────────────────────────────────────────────────
"""

import matplotlib
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np

# ── rcParams ──────────────────────────────────────────────────────────────────

RC = {
    # font
    'font.family':          'serif',
    'font.serif':           ['Times New Roman', 'DejaVu Serif'],
    'font.size':            9,
    # axes labels / title
    'axes.titlesize':       9,
    'axes.labelsize':       9,
    # ticks
    'xtick.labelsize':      8,
    'ytick.labelsize':      8,
    'xtick.major.width':    0.6,
    'ytick.major.width':    0.6,
    'xtick.major.size':     3,
    'ytick.major.size':     3,
    'xtick.minor.width':    0.4,
    'ytick.minor.width':    0.4,
    'xtick.minor.size':     1.5,
    'ytick.minor.size':     1.5,
    # axes frame
    'axes.linewidth':       0.6,
    # lines / markers
    'lines.linewidth':      1.0,
    'lines.markersize':     4,
    # legend
    'legend.fontsize':      7.5,
    'legend.title_fontsize':7.0,
    'legend.frameon':       True,
    'legend.edgecolor':     '0.5',
    'legend.framealpha':    1.0,
    'legend.borderpad':     0.4,
    'legend.labelspacing':  0.25,
    'legend.handlelength':  1.6,
    'legend.handletextpad': 0.4,
    # grid
    'grid.linewidth':       0.4,
    'grid.color':           '0.75',
    'grid.linestyle':       ':',
    # figure / save
    'figure.dpi':           300,
    'savefig.dpi':          300,
}


def apply():
    """Apply ACM rcParams globally. Call once at the top of your script."""
    plt.rcParams.update(RC)


# ── figure factories ──────────────────────────────────────────────────────────

# ACM SigConf column widths (inches)
W_SINGLE = 3.33
W_DOUBLE = 7.00
H_DEFAULT = 2.6


def fig_single(h=H_DEFAULT, **kwargs):
    """Return (fig, ax) sized for a single ACM column (3.33 in)."""
    fig, ax = plt.subplots(figsize=(W_SINGLE, h), **kwargs)
    _style_ax(ax)
    return fig, ax


def fig_double(h=H_DEFAULT, **kwargs):
    """Return (fig, ax) sized for a double ACM column (7.00 in)."""
    fig, ax = plt.subplots(figsize=(W_DOUBLE, h), **kwargs)
    _style_ax(ax)
    return fig, ax


def fig_subplots(nrows=1, ncols=2, h=H_DEFAULT, double=True, **kwargs):
    """Return (fig, axes) for a multi-panel figure.

    double=True  → total width = W_DOUBLE
    double=False → total width = W_SINGLE
    """
    w = W_DOUBLE if double else W_SINGLE
    fig, axes = plt.subplots(nrows, ncols, figsize=(w, h), **kwargs)
    for ax in np.array(axes).flatten():
        _style_ax(ax)
    return fig, axes


def _style_ax(ax):
    """Remove top/right spines (applied automatically by fig factories)."""
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)


# ── series style catalogue ────────────────────────────────────────────────────
# 8 entries — enough for most multi-series plots.
# Each dict can be unpacked directly into ax.plot / ax.errorbar.
# Marker + linestyle pairs are chosen to remain distinct in greyscale print.

COLORS = [
    '#7b2d8b',  # purple
    '#2e8b57',  # green
    '#d2691e',  # orange
    '#b22222',  # red
    '#1a5276',  # navy
    '#c71585',  # magenta
    '#4e6b3a',  # olive
    '#555555',  # grey
]

MARKERS = ['o', '^', 's', 'D', 'v', 'P', 'X', 'h']
LINES   = ['-', '--', '-', '--', '-', '--', '-', '--']

# Pre-built style dicts (unpack with **)
STYLES = [
    dict(marker=MARKERS[i], linestyle=LINES[i], color=COLORS[i],
         capsize=2.5, capthick=0.7, elinewidth=0.7, zorder=3)
    for i in range(len(COLORS))
]

# ── save kwargs ───────────────────────────────────────────────────────────────

SAVEFIG = dict(dpi=300, bbox_inches='tight', pad_inches=0.03)

# ── axis helpers ──────────────────────────────────────────────────────────────

def set_log2_xticks(ax, values):
    """Set a log-2 x-axis with plain integer labels (good for thread counts)."""
    ax.set_xscale('log', base=2)
    ax.set_xticks(values)
    ax.xaxis.set_major_formatter(ticker.ScalarFormatter())


def add_minor_yticks(ax, n=2):
    """Add n minor ticks between each pair of major y-ticks."""
    ax.yaxis.set_minor_locator(ticker.AutoMinorLocator(n))


def grid(ax, which='major'):
    """Enable dotted grid on the given axis."""
    ax.grid(True, which=which)


def finalize(ax, xlabel=None, ylabel=None, title=None,
             legend_loc='best', legend_title=None,
             ylim_zero=True, grid_on=True):
    """Convenience: set labels, legend, grid, and y-floor in one call."""
    if xlabel:
        ax.set_xlabel(xlabel)
    if ylabel:
        ax.set_ylabel(ylabel)
    if title:
        ax.set_title(title, pad=4)
    if ylim_zero:
        ax.set_ylim(bottom=0)
    if grid_on:
        grid(ax)
    ax.legend(loc=legend_loc, title=legend_title)


# ── tight layout wrapper ──────────────────────────────────────────────────────

def tight(fig, pad=0.4):
    """Apply tight_layout with ACM-appropriate padding."""
    fig.tight_layout(pad=pad)
