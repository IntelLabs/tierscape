

import argparse
import matplotlib.pyplot as plt
import numpy as np
import os
import math
import pandas as pd

import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import matplotlib.ticker as ticker
import matplotlib.ticker as plticker

# add a library path
import sys
sys.path.append(".")
from plot_utils import *

# Define the number of shades
num_shades = 8

# Create a grayscale color map
cmap = mcolors.LinearSegmentedColormap.from_list(
    'grayscale', [(i/num_shades, i/num_shades, i/num_shades) for i in range(num_shades)])
# Define a list of markers for each line
markers = ['o', 's', 'd', '^', '*', 'x', 'p', 'h']

# Define the colors of the colormap
colors = [(1.0, 0.0, 0.0), (1.0, 0.5, 0.0), (1.0, 1.0, 0.0),
          (0.0, 1.0, 0.0), (0.0, 1.0, 1.0), (0.0, 0.0, 1.0),
          (0.5, 0.0, 1.0), (1.0, 0.0, 1.0)]

# Create the colormap using the LinearSegmentedColormap class
cmap_rb = mcolors.LinearSegmentedColormap.from_list('red_blue_colormap', colors, N=num_shades)

# ============================
num_shades = 8

# Create a list of RGBA values that goes from red to blue with varying shades
colors = [(1.0, 0.0, 0.0, i/num_shades) for i in range(num_shades)] + \
         [(0.0, 0.0, 1.0, i/num_shades) for i in range(num_shades)]

# Create the colormap using the ListedColormap class
cmap_rgb2 = mcolors.ListedColormap(colors, N=num_shades*2)

# ============================
# create the argument parser
parser = argparse.ArgumentParser(description="Read contents of input file and write to output file")
# add the input file argument
parser.add_argument('--input', '-i', metavar='input_file', dest='input_file', required=True, help='input file path')
# add the output file argument
parser.add_argument('--stats', '-s', metavar='stats', dest='stats', required=False, help='stats to plot: \'nr_c_size\', \'nr_pages\', \'faults\'',default='nr_pages' )
parser.add_argument('--limit', '-l', metavar='limit', dest='limit', required=False, help='Limit the y-axis to the specified value', default=0                    ,type=int)
parser.add_argument('--plot_avg', '-a', metavar='plot_avg', dest='plot_avg', required=False, help='Limit the y-axis to the specified value', default=0                    ,type=int)
parser.add_argument('--pdf', '-p', metavar='plot_pdf', dest='plot_pdf', default=0, required=False, help='Enable PDF saving',type=int)
parser.add_argument('--plot_last_tier', '-lt', metavar='plot_last_tier', dest='plot_last_tier', required=False, help='Plot the last tier or not', default=1                    ,type=int)
parser.add_argument('--plot_log', '-lg', metavar='plot_log', dest='plot_log', required=False, help='Plot the last tier or not', default=0                    ,type=int)


# parse the arguments
args = parser.parse_args()

stats = args.stats
directory = os.path.dirname(args.input_file)

# if input file does not exist, exit
if not os.path.exists(args.input_file):
    # print scriptname and input file missing
    print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
    exit(1)


if args.plot_pdf==0:
    print("Exiting without plotting")
    exit(0)

# -------------------------------------

# define the stats to extract
available_stats = ['nr_c_size', 'nr_pages', 'faults', 'BS']

if stats not in available_stats:
    print(f'Error: {stats} is not a valid stat to plot')
    print(f'Available stats: {available_stats}')
    exit(1)
print(f'Plotting {stats} from {args.input_file}')
stats_to_extract = [stats]
pool_config = ['BS', 'type', 'comp']


# ======================READ TIER DATA========
# read the input file and extract the stats for each tier
tier_stats = {}
tier_config = {}
# # =============== GetVMRSS=================
# This is in MB now
ops = read_array_from_file(f"{directory}/plots/raw/dram_vmrss", data_type=int)
# multiple by 1024
ops = [x*1024 for x in ops]
stacked_df = pd.DataFrame([ops])
ops = read_array_from_file(f"{directory}/plots/raw/optane_vmrss", data_type=int)
ops = [x*1024 for x in ops]
stacked_opane_df = pd.DataFrame([ops])
print("VmRSS LEN", len(ops))

stacked_df = pd.concat([stacked_df, stacked_opane_df], ignore_index=True)
# # ==========================================

# ensure {directory}/plots/raw/tier/total_tiers exists
if not os.path.exists(f"{directory}/plots/raw/tier/total_tiers"):
    print(f"Error: {directory}/plots/raw/tier/total_tiers does not exist")
    exit(1)
# ensure {directory}/plots/raw/tier/total_compressed_tiers exists
if not os.path.exists(f"{directory}/plots/raw/tier/total_compressed_tiers"):
    print(f"Error: {directory}/plots/raw/tier/total_compressed_tiers does not exist")
    exit(1)


total_tiers=read_array_from_file(f"{directory}/plots/raw/tier/total_tiers", data_type=int)[0]
print("Total tiers DETECTED", total_tiers)
total_compressed_tiers=read_array_from_file(f"{directory}/plots/raw/tier/total_compressed_tiers", data_type=int)[0]
print("Total compressed tiers DETECTED", total_compressed_tiers)

# ======================PARSE TIER DATA========
for tier_id in range(total_tiers-total_compressed_tiers, total_tiers,1):
    label = f'tier {tier_id}'
    plot_data = read_array_from_file(f"{directory}/plots/raw/tier/tier_{tier_id}/{stats}", data_type=int)
    if len(plot_data) < len(ops):
        plot_data=[0]*(len(ops)-len(plot_data)) + plot_data
    else:
        plot_data=plot_data[:len(ops)]

    color = colors[tier_id % len(colors)]
    x = range(len(plot_data))
    x = np.arange(len(plot_data))
    
    if args.plot_avg==1:
        plot_data = running_average(plot_data)
    if stats == "nr_c_size":
        print("Convert Bytes to KB")
        # Bytes to KB
        plot_data = [x / 1024 for x in plot_data] 
    if stats == "nr_pages":
        # 4K pages to KB
        print("plot_stacked.py: Convert 4K pages to KB")
        plot_data = [x *4 for x in plot_data] 
        # plot_data = [x *1024 for x in plot_data] 

    try:
        new_df = pd.DataFrame([plot_data], columns=stacked_df.columns)
        stacked_df = pd.concat([stacked_df, new_df], ignore_index=True)
    except Exception as e:
        print(e)
        continue


# # ================================ Stacked PLOT for nr_pages ================================


stacked_df=stacked_df.T
if args.plot_avg==1:
    print("Plotting AVERAGE")

# print(stacked_df)

def plot_dataframe(orig_stacked_df,is_int_axis,is_norm,ylabel):
    # new figure
    # fig, ax = plt.subplots(figsize=(10, 7))
    fig, ax = plt.subplots()
    # print(df_normalized)
    # ax = df_plot_data.plot.bar(stacked=True,width=.9999,edgecolor='none',figsize=(10, 7),rot=0,cmap="coolwarm_r")
    ax = orig_stacked_df.plot.bar(stacked=True,width=.9999,edgecolor='none',rot=0,cmap="coolwarm_r")
    
    # show only 10 ticks on x-axis
    # ax.xaxis.set_major_locator(ticker.MaxNLocator(10))
    # start x-axis with 0
    # ax.set_xlim(left=0)
    


    # plt.xticks(arr, ['0', '100', '200', '300', '400', '500'])  # Example custom labels
    # print(plt.xticks())

    
    # remove legend
    ax.legend().remove()
    # ax.legend()
    ax.legend(legend_txt, loc='upper center', bbox_to_anchor=(0.45, 1.1), ncol=6,fontsize=14)
            
    # ax.legend().remove()
    # Add labels and title
    plt.xlabel('Time in seconds',fontsize=lfont)
    if args.plot_avg==1:
        plt.ylabel(f'{ylabel} (Running Avg)',fontsize=lfont)
    else:
        plt.ylabel(f'{ylabel}',fontsize=lfont)

    # Get the existing x-ticks
    
    
    # reduce number of ticks
    # ax.locator_params(axis='x', nbins=10)
    # loc = plticker.MultipleLocator(base=4) # this locator puts ticks at regular intervals
    # ax.xaxis.set_major_locator(loc)
    
    ticks_loc, ticks_labels = plt.xticks()
    plt.xticks(ticks_loc, np.array(ticks_loc*10).astype(int) ,rotation=0,fontsize=12)

    plt.locator_params(axis='y', nbins=9)
    plt.locator_params(axis='x', nbins=6)
    plt.xticks(plt.xticks()[0], ['0', '100', '200', '300', '400', '500'])  # Example custom labels
        
    format_axis(ax,lfont=lfont)
    if is_int_axis==1:
        fix_axis_int(ax)
    else:
        fix_axis_percentage(ax)

    
    if not os.path.exists(f"{directory}/plots"):
        os.makedirs(f"{directory}/plots")
    global stats
    
    local_stats=stats    
    if is_norm==1:
        local_stats=stats+"_norm"

    if(args.plot_pdf==1):
        plt.savefig(f"{directory}/plots/plot_tier_stacked_{local_stats}.png", dpi=300,bbox_inches="tight")
    elif(args.plot_pdf==2):
        plt.savefig(f"{directory}/plots/plot_tier_stacked_{local_stats}.pdf", dpi=100,bbox_inches="tight")
    else:
        print("Exiting without plotting")

if stats=="nr_pages":
    df_cumulative = stacked_df.cumsum()
    orig_stacked_df=stacked_df.copy()
    # Normalize the values to percentages
    row_totals = stacked_df.sum(axis=1)
    df_normalized = stacked_df.div(row_totals, axis=0) * 100
    df_normalized = df_normalized.fillna(0)

    # df_plot_data=df_normalized.copy()
    
    # divide by 1024
    orig_stacked_df = orig_stacked_df/(1024*1024)
    plot_dataframe(orig_stacked_df,1,0,"Size in GB")
    # plot_dataframe(orig_stacked_df,1,0,"Size in KB")
    # plot_dataframe(df_normalized,0,1,"% of data in Tiers")
    # 
        
    # print(orig_stacked_df)
else:
    print(f"Unsupported stats for stacked plots {stats}")


