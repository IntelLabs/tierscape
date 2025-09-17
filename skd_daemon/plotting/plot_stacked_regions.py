
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
from matplotlib.colors import BoundaryNorm
import matplotlib.patches as mpatches

import sys
sys.path.append(".")
from plot_utils import *


# ============================
# create the argument parser
parser = argparse.ArgumentParser(description="Read contents of input file and write to output file")
# add the input file argument
parser.add_argument('--input', '-i', metavar='input_file', dest='input_file', required=True, help='input file path')
# add the output file argument
parser.add_argument('--sort', '-st', metavar='plot_sorted', dest='plot_sorted', required=False, help='Plot sorted regions', default=0   ,type=int)
parser.add_argument('--pdf', '-p', metavar='plot_pdf', dest='plot_pdf', default=0, required=False, help='Enable PDF saving',type=int)
parser.add_argument('--tsleep', '-ts', metavar='trend_sleep_duration', dest='trend_sleep_duration', default=2, required=False, type=int)


# parse the arguments
args = parser.parse_args()



if args.input_file is None:
    print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
    exit(1)
else:
    print(f"=====================\nPlotting {os.path.basename(__file__)}")

trend_sleep_duration = args.trend_sleep_duration

stats="nr_pages"
directory = os.path.dirname(args.input_file)

if args.plot_pdf==0:
    print("Exiting without plotting")
    exit(0)
else:
    print(f"=====================\nPlotting {os.path.basename(__file__)}")
# -------------------------------------



# ======================READ REGION DATA========

start_addr=0
end_addr=0

regions_data={}
regions_data["time_id"]=[]
regions_data["region_id"]=[]
regions_data["curr_tier_id"]=[]
regions_data["dst_tier_id"]=[]
regions_data["hotness"]=[]



with open(args.input_file, 'r') as f:
    for line in f:
        if line.startswith('INFO'):
            start_addr = int(line.split(" ")[4])
            end_addr = int(line.split(" ")[6])

        else:
            # extract the stats for the current tier
            larr = line.split(" ")
            time_id=int(larr[0])
            region_id = int(larr[1])
            curr_tier_id = int(larr[2])
            dst_tier_id = int(larr[3])
            hotness=float(larr[4])

            hotness = min(100,hotness)
            
            curr_tier_id = 0 if curr_tier_id == -1 else curr_tier_id
            dst_tier_id = 0 if dst_tier_id == -1 else dst_tier_id

            regions_data["time_id"].append(time_id)
            regions_data["region_id"].append(region_id)
            regions_data["curr_tier_id"].append(curr_tier_id)
            regions_data["dst_tier_id"].append(dst_tier_id)
            regions_data["hotness"].append(hotness)
            
           

complete_df = pd.DataFrame(regions_data)

# print unique entires in curr_tier_id and count along side
print(complete_df["curr_tier_id"].value_counts().sort_index())
print(complete_df["dst_tier_id"].value_counts().sort_index())






def function_plot_data(imshow_data, total_tiers, is_sorted=False, stats_name=""):
    fig, ax = plt.subplots(dpi=100)
    cmap_color="coolwarm_r"

    if stats_name == "hotness":
        cmap_color="coolwarm"


    plt.imshow(imshow_data, cmap=cmap_color,aspect='auto')
    if stats_name == "hotness":
        plt.colorbar(label='Hotness', shrink=1)


    format_axis(ax,lfont=lfont-10)
    
    if is_sorted==True:
        ytick_labels = ["0%","20%","40%","60%","80%","100%"]
        plt.yticks(np.linspace(0, imshow_data.shape[0] - 1, num=6), ytick_labels)
        plt.ylabel('Percentage of data', fontsize=lfont)
    else:
        ytick_labels = [hex(x) for x in np.linspace(start_addr, end_addr, num=10).astype(int)]
        plt.yticks(np.linspace(0, imshow_data.shape[0] - 1, num=10), ytick_labels,fontsize=lfont-10)
        plt.ylabel('Address space ', fontsize=lfont-6)

    plt.gca().invert_yaxis()

    if stats_name !="hotness":
        cmap = plt.get_cmap(cmap_color, total_tiers)    
        colors = [cmap(i) for i in range(total_tiers)]
        labels = [f'Tier {i}' for i in range(total_tiers)]

        patches = [mpatches.Patch(color=colors[i], label=labels[i]) for i in range(total_tiers)]
        ax.legend(handles=patches, loc='upper center', bbox_to_anchor=(0.35, 1.24),fontsize=16, ncol=3)
        # ax.legend(handles=patches, loc='best',fontsize=16, ncol= total_tiers % 3)


    # windows = imshow_data.shape[1]
    # x_ticks = np.linspace(0, windows - 1, num=windows)
    # plt.xticks(x_ticks, fontsize=lfont-6)
    # ax.set_xticklabels([int(x * 10) for x in x_ticks])
    plt.xlabel('Profile Windows', fontsize=lfont-6)

    loc = plticker.MultipleLocator(base=2) # this locator puts ticks at regular intervals
    ax.xaxis.set_major_locator(loc)


    if not os.path.exists(f"{directory}/plots"):
        os.makedirs(f"{directory}/plots")

    filename=f"plot_regions_{stats_name}"
    if is_sorted==True:
        filename+="_sorted"
    if(args.plot_pdf==1):
        print("Saving png to: ", directory + "/plots/" + filename + ".png")
        plt.savefig(f"{directory}/plots/{filename}.png", dpi=100,bbox_inches="tight")
    elif(args.plot_pdf==2):
        print("Saving pdf to: ", directory + "/plots/" + filename + ".pdf")
        plt.savefig(f"{directory}/plots/{filename}.pdf", dpi=100,bbox_inches="tight")
    else:
        print("Not plotting")



# # sorted =============
total_tiers=len(np.unique(complete_df["curr_tier_id"]))
total_tiers=len(np.unique(complete_df["dst_tier_id"]))

imshow_data = complete_df.pivot(index='region_id', columns='time_id', values='curr_tier_id')
function_plot_data(imshow_data, total_tiers, False,"curr_tier")
imshow_data_sorted = imshow_data.apply(lambda x: sorted(x), axis=0)
function_plot_data(imshow_data_sorted, total_tiers, True, "curr_tier")


total_tiers=len(np.unique(complete_df["dst_tier_id"]))
imshow_data = complete_df.pivot(index='region_id', columns='time_id', values='dst_tier_id')
function_plot_data(imshow_data, total_tiers, False,"dst_tier")
imshow_data_sorted = imshow_data.apply(lambda x: sorted(x), axis=0)
function_plot_data(imshow_data_sorted, total_tiers, True, "dst_tier")


imshow_data = complete_df.pivot(index='region_id', columns='time_id', values='hotness')
function_plot_data(imshow_data, total_tiers, False,"hotness")

# # get hotness in an array
# hotness_data = complete_df["hotness"].values
# nr_regions = len(complete_df["region_id"].unique())
# nr_windows = len(complete_df["time_id"].unique())
# hotness_data = hotness_data.reshape(nr_regions, nr_windows)
# # plot imshow
# plt.imshow(hotness_data, cmap='coolwarm', aspect='auto', vmin=0, vmax=10)
# # invert y axis
# plt.gca().invert_yaxis()
# # add cbar
# plt.colorbar(label='Hotness', shrink=1)
# plt.ylabel('Address space (Regions)', fontsize=lfont-10)
# plt.xlabel('Profile Windows', fontsize=lfont-10)
# # save the plot
# plt.savefig(f"{directory}/plots/plot_region_hotness2.png", dpi=100, bbox_inches="tight")

