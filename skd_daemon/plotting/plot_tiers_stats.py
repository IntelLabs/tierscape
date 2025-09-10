


import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import pandas as pd
import seaborn as sns
import argparse
import os
import re

import sys
sys.path.append(".")
from plot_utils import *
from tier_config_simple import get_complete_uncompressed_tiers_info, get_complete_compressed_tiers_info


# create the argument parser
parser = argparse.ArgumentParser(description="Read contents of input file and write to output file")
# add the input file argument
parser.add_argument('--input', '-i', metavar='input_file', dest='input_file', required=True, help='input file path')
parser.add_argument('--pdf', '-p', metavar='plot_pdf', dest='plot_pdf', default=0, required=False, help='Enable PDF saving',type=int)
args = parser.parse_args()

directory = os.path.dirname(args.input_file)

# if input file is not provided, exit
if args.input_file is None:
    print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
    exit(1)
else:
    # check if the file exists
    if not os.path.isfile(args.input_file):
        print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
        exit(1)

print(f"=====================\nPlotting {os.path.basename(__file__)}")

# Load tier configuration from shared config
try:
    uncompressed_tiers = get_complete_uncompressed_tiers_info()
    compressed_tiers = get_complete_compressed_tiers_info()
    print(f"Loaded uncompressed tiers config: {uncompressed_tiers}")
    print(f"Loaded compressed tiers config: {compressed_tiers}")

    # For this plot, we only want compressed tiers
    # Use their offset (0, 1, 2) as they appear in pool_id
    valid_tier_ids = [tier['offset'] for tier in compressed_tiers]
    
    # Create mapping for display purposes (compressed tiers only)
    tier_id_mapping = {}
    for tier in compressed_tiers:
        tier_id_mapping[tier['offset']] = f"Compressed-{tier['offset']} ({tier['compressor']}, backing={tier['backing_store']})"
    
    # For backing store analysis, we still need to know which uncompressed tiers are DRAM/OPTANE
    fast_tier_idxes = [tier['virt_id'] for tier in uncompressed_tiers if tier['mem_type'] == 'DRAM']
    slow_tier_idxes = [tier['virt_id'] for tier in uncompressed_tiers if tier['mem_type'] == 'OPTANE']

    print(f"Valid tier IDs for filtering (compressed only): {valid_tier_ids}")
    print("Compressed tier ID mapping:")
    for tid in sorted(tier_id_mapping.keys()):
        description = tier_id_mapping[tid]
        print(f"  pool_id {tid}: {description}")
    print(f"Fast tier indices (DRAM): {fast_tier_idxes}")
    print(f"Slow tier indices (OPTANE): {slow_tier_idxes}")
except Exception as e:
    print(f"Warning: Failed to load tier config: {e}")
    # fatal error
    print("Error: Could not determine tier indices from config. Exiting.")
    exit(1)


data = []
with open(args.input_file, "r") as f:
    for line in f:
        tokens = line.replace(":","").strip().split()
        cleaned = [token.rstrip(",") for token in tokens]
        # print(cleaned)
        try:
            entry = {cleaned[i]: cleaned[i+1] for i in range(0, len(cleaned), 2)}
        except IndexError:
            continue  # Skip if there's an odd number of tokens
        
        # Convert numeric fields to appropriate types
        for k, v in entry.items():
            try:
                entry[k] = int(v)
            except ValueError:
                try:
                    entry[k] = float(v)
                except ValueError:
                    pass  # keep string if not numeric
        data.append(entry)

df = pd.DataFrame(data)

# Filter data to only include configured compressed tiers
initial_rows = len(df)
df = df[df['pool_id'].isin(valid_tier_ids)]
filtered_rows = len(df)
print(f"Filtered data: {initial_rows} -> {filtered_rows} rows (kept only compressed tiers: {valid_tier_ids})")

# for the col timestamp, subtract from the first timestamp
if len(df) > 0:
    df['timestamp'] = df['timestamp'] - df['timestamp'].iloc[0]
    # convert to int
    df['timestamp'] = df['timestamp'].astype(int)
else:
    print("Warning: No data remaining after filtering for compressed tiers")
    exit(1)


for pid in df['pool_id'].unique():
    try:
        # subtrace first faults entry from all entries
        df.loc[df['pool_id'] == pid, 'faults'] = df.loc[df['pool_id'] == pid, 'faults'] - df.loc[df['pool_id'] == pid, 'faults'].iloc[0]
    except IndexError:
        print(f"IndexError for pool_id {pid}. Skipping this pool_id.")
        continue

# add a column page_size, nr_pages mult by 4096
df['actual_size'] = df['nr_pages'] * 4096

# print(df.head())

# Set up plot styling
plt.style.use('default')
fig, axes = plt.subplots(2, 2, figsize=(12, 8))
fig.suptitle('Compressed Tiers Statistics', fontsize=14)

plot_configs = [
    ("nr_compressed_size", "Compressed Size", "bytes", axes[0,0]),
    ("nr_pages", "Number of Pages", "count", axes[0,1]), 
    ("faults", "Page Faults", "count", axes[1,0]),
    ("actual_size", "Actual Size", "bytes", axes[1,1])
]

# Create legend labels once
legend_labels = {}
for tier in compressed_tiers:
    pool_id = tier['offset']
    backing_name = 'DRAM' if tier['backing_store'] in fast_tier_idxes else 'OPTANE'
    legend_labels[pool_id] = f"T{pool_id}: {tier['compressor']}/{tier['pool_manager'][:3]}/{backing_name}"

# Plot each metric
for plot_data, title, unit_type, ax in plot_configs:
    # Plot lines for each pool_id
    for pool_id in sorted(df['pool_id'].unique()):
        pool_data = df[df['pool_id'] == pool_id]
        label = legend_labels.get(pool_id, f"T{pool_id}")
        ax.plot(pool_data['timestamp'], pool_data[plot_data], 
               marker='o', markersize=4, linewidth=2, label=label)
    
    ax.set_xlabel("Time (seconds)")
    ax.set_ylabel(title)
    ax.set_title(title)
    ax.grid(True, alpha=0.3)
    
    # Format y-axis based on data type
    if unit_type == "bytes":
        def size_formatter(x, pos):
            if x >= 1e9:
                return f'{x/1e9:.1f}G'
            elif x >= 1e6:
                return f'{x/1e6:.1f}M'
            elif x >= 1e3:
                return f'{x/1e3:.1f}K'
            else:
                return f'{x:.0f}'
        ax.yaxis.set_major_formatter(ticker.FuncFormatter(size_formatter))
    elif unit_type == "count":
        def count_formatter(x, pos):
            if x >= 1e6:
                return f'{x/1e6:.1f}M'
            elif x >= 1e3:
                return f'{x/1e3:.1f}K'
            else:
                return f'{x:.0f}'
        ax.yaxis.set_major_formatter(ticker.FuncFormatter(count_formatter))

# Add legend to the last subplot
axes[1,1].legend(loc='center left', bbox_to_anchor=(1, 0.5), frameon=True)

plt.tight_layout()
print("Saving PNG to", f"{directory}/plots/plot_zswap_all_metrics.png")
plt.savefig(f"{directory}/plots/plot_zswap_all_metrics.png", dpi=300, bbox_inches="tight")
plt.close()


# print(df)
# sumup nr_compressed_size based on backing store.. if 0 or  1 then in dram_usage 
# dram_usage = df[(df['backing_store'] == 0) | (df['backing_store'] == 1)][['timestamp','nr_compressed_size']]
# optane_usage = df[(df['backing_store'] == 2) | (df['backing_store'] == 3)][['timestamp','nr_compressed_size']]

# use fast_tier_idxes and slow_tier_idxes if available
if fast_tier_idxes is not None and slow_tier_idxes is not None and len(fast_tier_idxes) > 0 and len(slow_tier_idxes) > 0:
    dram_usage = df[df['backing_store'].isin(fast_tier_idxes)][['timestamp','nr_compressed_size']]
    optane_usage = df[df['backing_store'].isin(slow_tier_idxes)][['timestamp','nr_compressed_size']]
else:
    # fatal error
    print("Error: Could not determine DRAM/OPTANE tier indices from config. Exiting.")
    exit(1)

# sum them based on timestamp
dram_usage = dram_usage.groupby('timestamp').sum()
optane_usage = optane_usage.groupby('timestamp').sum()

# reset index
dram_usage = dram_usage.reset_index()
optane_usage = optane_usage.reset_index()

# second col is in bytes, convert to MB
dram_usage['nr_compressed_size'] = dram_usage['nr_compressed_size'] / (1024 * 1024)
optane_usage['nr_compressed_size'] = optane_usage['nr_compressed_size'] / (1024 * 1024)


zswap_dram_usage = dram_usage['nr_compressed_size'].to_numpy()
zswap_optane_usage = optane_usage['nr_compressed_size'].to_numpy()


# print(zswap_dram_usage)
# print(zswap_optane_usage)

save_array_to_file( f"{directory}/plots/raw/zswap_dram_usage_mb",zswap_dram_usage)
save_array_to_file( f"{directory}/plots/raw/zswap_optane_usage_mb",zswap_optane_usage)

