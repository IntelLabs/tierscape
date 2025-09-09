#! /usr/bin/env python3

import pickle
import argparse
import matplotlib.pyplot as plt
import numpy as np
import os
import math
import pandas as pd

import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import matplotlib.ticker as ticker

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
parser.add_argument('--sort', '-st', metavar='plot_sorted', dest='plot_sorted', required=False, help='Plot sorted regions', default=0   ,type=int)
parser.add_argument('--limit', '-l', metavar='limit', dest='limit', required=False, help='Limit the y-axis to the specified value', default=100                    ,type=int)
parser.add_argument('--pdf', '-p', metavar='plot_pdf', dest='plot_pdf', default=1, required=False, help='Enable PDF saving',type=int)



# parse the arguments
args = parser.parse_args()

# ensure the input file exists
if not os.path.exists(args.input_file):
    # print scriptname and input file missing
    print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
    exit(1)


stats="nr_pages"
directory = os.path.dirname(args.input_file)

if args.plot_pdf==0:
    print("Exiting without plotting")
    exit(0)
# -------------------------------------

# define the stats to extract
available_stats = ['nr_c_size', 'nr_pages', 'faults', 'BS']
stats_to_extract = [stats]
pool_config = ['BS', 'type', 'comp']


# ======================READ REGION DATA========

regions_data={}
regions_data["time_id"]=[]
regions_data["region_id"]=[]
regions_data["curr_tier_id"]=[]
regions_data["dst_tier_id"]=[]
regions_data["hotness"]=[]


hotness_data=[]
start_addr=0
end_addr=0


complete_df = pd.DataFrame()
complete_hot_df = pd.DataFrame()
with open(args.input_file, 'r') as f:
    for line in f:
        if line.startswith('INFO'):
            start_addr = int(line.split(" ")[4])
            end_addr = int(line.split(" ")[6])
            if(len(hotness_data)> 0):
                if args.plot_sorted==1:
                    tier_data=sorted(tier_data)
                complete_hot_df = pd.concat([complete_hot_df, pd.DataFrame([hotness_data])], ignore_index=True)
                hotness_data=[]
        else:
            larr = line.split(" ")
            time_id=int(larr[0])
            region_id = int(larr[1])
            curr_tier_id = int(larr[2])
            dst_tier_id = int(larr[3])
            hotness=float(larr[4])

            if hotness > args.limit:
                hotness = args.limit
            
            curr_tier_id = 0 if curr_tier_id == -1 else curr_tier_id
            dst_tier_id = 0 if dst_tier_id == -1 else dst_tier_id

            regions_data["time_id"].append(time_id)
            regions_data["region_id"].append(region_id)
            regions_data["curr_tier_id"].append(curr_tier_id)
            regions_data["dst_tier_id"].append(dst_tier_id)
            regions_data["hotness"].append(hotness)
            

            hotness_data.append(hotness)

complete_df=complete_hot_df.T

uniq_cout_arr =  np.unique(complete_df.values, return_counts=True)

hotness_df = pd.DataFrame(regions_data)


# =========================
fig, ax = plt.subplots()

imshow_data = hotness_df.pivot(index='region_id', columns='time_id', values='hotness')
plt.imshow(imshow_data,  cmap='coolwarm',aspect='auto')
# plt.imshow(complete_df.values,  cmap='coolwarm',aspect='auto')

plt.colorbar()

print(plt.xlim())

if args.plot_sorted==0:
   
    # ytick_labels = [hex(x) for x in np.linspace(start_addr, end_addr, num=10).astype(int)]
    # plt.ylabel('Address space', fontsize=lfont)
    
    ytick_labels = [int(x/(1024*1024*1024)) for x in np.linspace(0, end_addr - start_addr, num=10).astype(int)]
    plt.yticks(np.linspace(0, complete_df.shape[0] - 1, num=10), ytick_labels)
    plt.ylabel('Memory in GB', fontsize=lfont)
    
    # Reversing the y-axis
    plt.gca().invert_yaxis()
else:
    ytick_labels = ["0%","20%","40%","60%","80%","100%"]
    plt.yticks(np.linspace(0, complete_df.shape[0] - 1, num=6), ytick_labels)
    plt.ylabel('Percentage of Data', fontsize=lfont)
    plt.gca().invert_yaxis()

plt.xlabel('Time in seconds', fontsize=lfont)

ax=plt.gca()    

# loc = plticker.MultipleLocator(base=2) # this locator puts ticks at regular intervals
# ax.xaxis.set_major_locator(loc)

# plt.xticks(np.linspace(0, 100, 600))  # Adjust the range as per your data


plt.locator_params(axis='y', nbins=5)

# print ylim
print("ylim", plt.ylim())

plt.grid(False)
format_axis(ax,lfont=lfont-6)
# ax.legend(, loc='upper center', bbox_to_anchor=(0.5, 1.122), ncol=5,fontsize=16)

if not os.path.exists(f"{directory}/plots"):
    os.makedirs(f"{directory}/plots")

filename="plot_tier_stacked_regions_hotness"
if args.plot_sorted==1:
    filename+="_sorted"

print(f"Plot saved to {directory}/{filename}.png")
if(args.plot_pdf==1):
    plt.savefig(f"{directory}/plots/{filename}.png", dpi=300,bbox_inches="tight")
elif(args.plot_pdf==2):
    plt.savefig(f"{directory}/plots/{filename}.pdf", dpi=100,bbox_inches="tight")
else:
    print("Not plotting hotness")

