#! /usr/bin/env python3

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
parser.add_argument('--pdf', '-p', metavar='plot_pdf', dest='plot_pdf', default=1, required=False, help='Enable PDF saving',type=int)

args = parser.parse_args()



if args.input_file is None:
    print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
    exit(1)
else:
    print(f"=====================\nPlotting {os.path.basename(__file__)}")

events_data=[]
regions_data=[]

directory = os.path.dirname(args.input_file)
nr_ilp_failed=0
with open(args.input_file, "r") as f:
    for line in f:
        if "Total events captured" in line:
            # replace multiple consecutive spaces with a single space
            line = ' '.join(line.split())
            cols = line.split(' ')
            
            
            events_captured = int(cols[9])
            regions = int(cols[5])
            
            events_data.append(events_captured)
            regions_data.append(regions)
        elif "ILP Failed" in line:
            nr_ilp_failed+=1
            

events_data=np.array(events_data)
regions_data=np.array(regions_data)


# print mean of all round to 2
# print("Len",len(events_data)," Mean of events",round(np.mean(events_data),2), "Mean of regions",round(np.mean(regions_data),2))

avg_events_data=running_average(events_data)
avg_regions_data=running_average(regions_data)


if not os.path.exists(f"{directory}/plots"):
    os.makedirs(f"{directory}/plots")
directory=f"{directory}/plots"

if nr_ilp_failed > 0:
    # create a file in plots 1_nr_ilp_failed_ilp_failed.txt
    with open(f"{directory}/1_{nr_ilp_failed}_ILP_FAILED.txt", "w") as f:
        f.write(f"Number of ILP Failed: {nr_ilp_failed}\n")

if args.plot_pdf==0:
    print("Exiting without plotting")
    exit(0)
    

# define the tick formatter function
def comma_format(x, pos):
    return "{:,.0f}".format(x)

# apply the formatter to the y-axis ticks
formatter = ticker.FuncFormatter(comma_format)

ax=plt.gca()

dram_x_data = np.linspace(0, (len(events_data)-1)*10, num=len(events_data))
optane_x_data = np.linspace(0, (len(regions_data)-1)*10, num=len(regions_data))

# print(x_data)
lns1=plt.plot(dram_x_data, events_data, label="Events Captured", color="red")
plt.ylabel('Events')

# plt.plot(optane_x_data, optane_usage_data, label="Optane", color="blue")
# plot regions_data on secondary axis
ax.twinx()
lns2=plt.plot(optane_x_data, regions_data, label="Regions", color="blue")
plt.ylabel('Regions')

plt.xlabel('Time in seconds')


# added these three lines
lns = lns1+lns2
labs = [l.get_label() for l in lns]
ax.legend(lns, labs, loc=0)

# plt.legend(["Events","Regions"])
# legends for both teh y =axis
# plt.legend(["Events","Regions"], loc='upper left')

ax.yaxis.set_major_formatter(formatter)
# plt.legend().remove()
plt.grid()
print("Saving plot to ", f"{directory}/plot_events.png")
plt.savefig(f"{directory}/plot_events.png", dpi=300,bbox_inches="tight")
