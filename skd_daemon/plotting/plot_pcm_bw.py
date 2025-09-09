


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


directory = os.path.dirname(args.input_file)    
directory=f"{directory}/plots"
if not os.path.exists(f"{directory}/plots"):
    os.makedirs(f"{directory}/plots")

# if input file is not provided, exit
if args.input_file is None:
    print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
    exit(1)

bandwidth_data={}

directory = os.path.dirname(args.input_file)
with open(args.input_file, "r") as f:
    for line in f:
        line = line.strip()
        # do not print empty lines
        if not line:
            continue
        # remove System and Throughput(MB/s) from the line
        line = line.replace("System","")
        line = line.replace("Throughput(MB/s)","")
        # replace consecutive spaces with a single space
        line = ' '.join(line.split())
        entry = line.split(':')
        entry = [x.strip() for x in entry]
        if len(entry) != 2:
            print("ERROR",line,entry)
            continue
        if entry[0] not in bandwidth_data:
            bandwidth_data[entry[0]] = []
        try:
            bandwidth_data[entry[0]].append(float(entry[1]))
        except ValueError:
            # print("ERROR",line,entry)
            continue

# `# print length of each key
# for key in bandwidth_data.keys():
#     print(f"Length of {key} is {len(bandwidth_data[key])}")`

# make them of same length, remove additional entries
min_len = min([len(bandwidth_data[key]) for key in bandwidth_data.keys()])
for key in bandwidth_data.keys():
    bandwidth_data[key] = bandwidth_data[key][:min_len]
    


df = pd.DataFrame(bandwidth_data)
# print(df)



def get_color_and_style(col):
    color="black"
    if "dram" in col.lower():
        color = "blue"
    elif "pmm" in col.lower():
        color = "red"
    else:
        color = "black"
    
    if "read" in col.lower():
        style = "-"
    elif "write" in col.lower():
        style = "--"
    else:
        style = "dotted"
    
    # print(f"Color: {color} Style: {style} for {col}" )
    return color, style
        
fig, ax = plt.subplots()

# dram entries line blue, pmm line read, else black
for i in range(len(df.columns)):
    col = df.columns[i]
    x_data = np.linspace(0, (len(df[col])-1)*trend_sleep_duration, num=len(df[col]))
    
    color, style = get_color_and_style(df.columns[i])
    if color=="black":
        # send it to back
        zorder=-1000
    else:
        zorder=100
    plt.plot(x_data, df[df.columns[i]], label=df.columns[i], color=color, linestyle=style,linewidth=2, alpha=0.6,zorder=zorder)
        
plt.xlabel('Time in seconds',fontsize=lfont-4)
plt.ylabel('Bandwidth in MB/s',fontsize=lfont-4)


# log scale
plt.yscale('log')

plt.legend(fontsize=lfont-12,loc='upper center', bbox_to_anchor=(.45,1.2), ncol=3)
# ax.yaxis.set_major_formatter(formatter)
plt.grid()
format_axis(ax,lfont=lfont-6)
print("Saving to ",f"{directory}/plots/plot_pcm_bw.png")
plt.savefig(f"{directory}/plots/plot_pcm_bw.png", dpi=300,bbox_inches="tight")

if args.plot_pdf==2:
    # save as pdf
    print("Saving to ",f"{directory}/plots/plot_pcm_bw.pdf")
    plt.savefig(f"{directory}/plots/plot_pcm_bw.pdf", dpi=300,bbox_inches="tight")

    

# # define the tick formatter function
# def comma_format(x, pos):
#     return "{:,.0f}".format(x)

# # apply the formatter to the y-axis ticks
# formatter = ticker.FuncFormatter(comma_format)

# ax=plt.gca()

# dram_x_data = np.linspace(0, (len(dram_usage_data)-1)*trend_sleep_duration, num=len(dram_usage_data))
# optane_x_data = np.linspace(0, (len(optane_usage_data)-1)*trend_sleep_duration, num=len(dram_usage_data))
# # print(x_data)
# plt.plot(dram_x_data, dram_usage_data, label="DRAM", color="blue")
# plt.plot(optane_x_data, optane_usage_data, label="Optane", color="red")


# plt.xlabel('Time in seconds',fontsize=lfont-4)
# plt.ylabel('VmRSS in MB',fontsize=lfont-4)

# plt.legend(fontsize=lfont-10)

# ax.yaxis.set_major_formatter(formatter)
# # plt.legend().remove()
# plt.grid()

# format_axis(ax,lfont=lfont-6)

# plt.savefig(f"{directory}/plot_numastat.png", dpi=300,bbox_inches="tight")

# # ===============================
# color_array=["blue","red","green","orange","purple","pink","brown"]
# style_arr=["-","--","-.",":","-","--","-."]

# # new figure., plot numa_node_data
# fig, ax = plt.subplots()
# # plot each numa node data
# for i in range(1, len(numa_node_data)+1):
#     # plot the data
#     x_data = np.linspace(0, (len(numa_node_data[i])-1)*trend_sleep_duration, num=len(numa_node_data[i]))
#     plt.plot(x_data, numa_node_data[i], label=f"Node {i-1}", color=color_array[i-1], linestyle=style_arr[i-1],linewidth=2)
# plt.xlabel('Time in seconds',fontsize=lfont-4)
# plt.ylabel('Size in MB',fontsize=lfont-4)
# plt.legend(fontsize=lfont-10)
# ax.yaxis.set_major_formatter(formatter)
# plt.grid()
# format_axis(ax,lfont=lfont-6)
# print("Saving to ",f"{directory}/plot_numastat_numa.png")
# plt.savefig(f"{directory}/plot_numastat_numa.png", dpi=300,bbox_inches="tight")
