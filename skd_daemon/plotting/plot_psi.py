#!/usr/bin/python3


import matplotlib.pyplot as plt
import argparse
import os
import matplotlib.ticker as ticker
import numpy as np

# add a library path
import sys
sys.path.append(".")
from plot_utils import *


# create the argument parser
parser = argparse.ArgumentParser(description="Read contents of input file and write to output file")
parser.add_argument('--input', '-i', metavar='input_file', dest='input_file', required=True, help='input file path')
parser.add_argument('--pdf', '-p', metavar='plot_pdf', dest='plot_pdf', default=0, required=False, help='Enable PDF saving',type=int)
parser.add_argument('--tsleep', '-ts', metavar='trend_sleep_duration', dest='trend_sleep_duration', default=2, required=False, type=int)

args = parser.parse_args()

trend_sleep_duration = args.trend_sleep_duration

if args.input_file is None:
    print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
    exit(1)
else:
    print(f"=====================\nPlotting {os.path.basename(__file__)}")


# dram_usage_data=[]
# optane_usage_data=[]
# total_usage_data=[]

some_data={}
full_data={}

some_data["avg10"]=[]
some_data["avg60"]=[]
some_data["avg300"]=[]
some_data["total"]=[]

full_data["avg10"]=[]
full_data["avg60"]=[]
full_data["avg300"]=[]
full_data["total"]=[]


directory = os.path.dirname(args.input_file)
with open(args.input_file, "r") as f:
    for line in f:
            # replace multiple consecutive spaces with a single space
        cols = line.split(' ')
        for i in [1,2,3,4]:
            [key,val]= cols[i].split("=")
            # print(key,val)
            if "some" in line:
                some_data[key].append(float(val))
            else:
                full_data[key].append(float(val))



# # subtract the first entry in some_data["total"] from the complete array
full_data["total"] = [x - full_data["total"][0] for x in full_data["total"]]
some_data["total"] = [x - some_data["total"][0] for x in some_data["total"]]

# # convert total as diff of previous element, for the first, set it to 0
# full_data["total"] = [full_data["total"][i] - full_data["total"][i-1] for i in range(1,len(full_data["total"]))]
# full_data["total"].insert(0,0)

# some_data["total"] = [some_data["total"][i] - some_data["total"][i-1] for i in range(1,len(some_data["total"]))]
# some_data["total"].insert(0,0)

# print(some_data)


# plot some data and full_Data, avg as lines, total as bar plot in same plot
# define the tick formatter function


x_data = np.linspace(0, (len(some_data["avg10"])-1)*trend_sleep_duration, num=len(some_data["avg10"]))

# sns.lineplot(x=x_data, y=some_data["avg10"], label="Some avg10",linestyle="--"

sns.lineplot(x=x_data, y=some_data["avg10"], label="Some avg10 stall fraction",linestyle="--"             ,color="red")
# sns.lineplot(x=x_data, y=some_data["avg60"], label="Some avg60",linestyle="--"             ,color="red")
# sns.lineplot(x=x_data, y=some_data["avg300"], label="Some avg300",linestyle="--"             ,color="blue")

sns.lineplot(x=x_data, y=full_data["avg10"], label="Full avg10 stall fraction",color="red")
# sns.lineplot(x=x_data, y=full_data["avg60"], label="Full avg60")
# sns.lineplot(x=x_data, y=full_data["avg300"], label="Full avg300")


# ylabel stall fraction
plt.ylabel('Memory Stall Fraction')
plt.xlabel('Time in seconds')
# multiply xticks by 10
ax=plt.gca()
format_axis(ax,16)

# format ax as percentage
ax.yaxis.set_major_formatter(ticker.FuncFormatter(lambda x, _: '{:.2%}'.format(x/100)))
ax.legend(loc='upper right', bbox_to_anchor=(.5,1.2), ncol=1,fontsize=12)
# twinx
ax2 = plt.twinx()

# plot total as bar plot on ax2
# ax2.bar(x_data, some_data["total"], label="Some total", color="black", alpha=.2,width=2)
# line plot
sns.lineplot(x=x_data, y=some_data["total"], label="Some stall time", color="black", alpha=1,linewidth=2,linestyle="dotted")
sns.lineplot(x=x_data, y=full_data["total"], label="Full stall time", color="black", alpha=1,linewidth=2,linestyle="dashdot")

def comma_format(x, pos):
    return "{:,.2f}".format(x/1000000)
ax2.yaxis.set_major_formatter(ticker.FuncFormatter(comma_format))
ax2.set_ylabel('Stall time in sec (Cummulative)', fontsize=16)
ax2.tick_params(axis='both', which='major', labelsize=16)
ax2.legend(loc='upper left', bbox_to_anchor=(0.5, 1.2), ncol=1,fontsize=12)

# format_axis(ax2,16)
# set ticks font as 16

print(f"Saving plot to {directory}/plots/plot_psi.png")
plt.savefig(f"{directory}/plots/plot_psi.png", dpi=300,bbox_inches="tight")
