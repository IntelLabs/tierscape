


import matplotlib.pyplot as plt
import argparse
import os
import matplotlib.ticker as ticker
import numpy as np

# add a library path
import sys
sys.path.append(".")
from plot_utils import *
from tier_config_simple import get_dram_optane_info


# create the argument parser
parser = argparse.ArgumentParser(description="Read contents of input file and write to output file")
parser.add_argument('--input', '-i', metavar='input_file', dest='input_file', required=True, help='input file path')
parser.add_argument('--pdf', '-p', metavar='plot_pdf', dest='plot_pdf', default=0, required=False, help='Enable PDF saving',type=int)
parser.add_argument('--tsleep', '-ts', metavar='trend_sleep_duration', dest='trend_sleep_duration', default=2, required=False, type=int)

args = parser.parse_args()

trend_sleep_duration = args.trend_sleep_duration

# Load DRAM/OPTANE configuration from shared config
try:
    dram_optane_config = get_dram_optane_info()
    print(f"Loaded DRAM/OPTANE config: {dram_optane_config}")

    total_byte_tiers = len(dram_optane_config)
    fast_tier_idxes = [tier['virt_id'] for tier in dram_optane_config.values() if tier and tier['mem_type'] == 0]  # DRAM
    slow_tier_idxes = [tier['virt_id'] for tier in dram_optane_config.values() if tier and tier['mem_type'] == 1]  # OPTANE

    print("Total byte-addressable tiers:", total_byte_tiers)
    print(f"Fast tier indices (DRAM): {fast_tier_idxes}")
    print(f"Slow tier indices (OPTANE): {slow_tier_idxes}")
except Exception as e:
    print(f"Warning: Failed to load tier config: {e}")
    dram_optane_config = None

# if input file is not provided, exit
if args.input_file is None:
    print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
    exit(1)
else:
    print(f"=====================\nPlotting {os.path.basename(__file__)}")

dram_usage_data=[]
optane_usage_data=[]
total_usage_data=[]

numa_node_data={}

directory = os.path.dirname(args.input_file)
with open(args.input_file, "r") as f:
    for line in f:
        if "Total" in line:
            # replace multiple consecutive spaces with a single space
            line = ' '.join(line.split())
            cols = line.split(' ')
            
            # try:
            #     if cols[5]==0:
            #         continue # the pgrogram exited and this are stray entries
            # except IndexError:
            #     print("Index error in line:", line)
            #     print(cols)
            #     exit(1)
            
            try:
                for idx in fast_tier_idxes:
                    dram_data = int(cols[idx+1])
                for idx in slow_tier_idxes:
                    optane_data = int(cols[idx+1])

                total_data = int(cols[total_byte_tiers+1])
            except Exception as e:
                print(f"Error parsing line: {line} {cols} {total_byte_tiers+1} with error {e}")
                # print stack trace
                import traceback
                traceback.print_exc()
                exit(1)

            # dram_data = int(cols[1]) + int(cols[2])
            # optane_data = int(cols[3]) + int(cols[4])
            # total_data = int(cols[5])
            
            # loop thrugh all cols except last
            for i in range(1, len(cols)-1):
                # append to numa_node_data
                if i not in numa_node_data:
                    numa_node_data[i] = []
                numa_node_data[i].append(int(cols[i]))
                
           
            dram_usage_data.append(dram_data)
            optane_usage_data.append(optane_data)
            total_usage_data.append(total_data)
            

dram_usage_data=np.array(dram_usage_data)
optane_usage_data=np.array(optane_usage_data)
total_usage_data=np.array(total_usage_data)

# print numa_node_stats
# print(numa_node_data)

# THis is MB. Convert to GB and save the results.
dram_tco_data=(dram_usage_data/(1024))*3
optane_tco_daa = (optane_usage_data/(1024))
total_tco_data = dram_tco_data + optane_tco_daa

# print mean of all round to 2
# print("Len",len(dram_tco_data)," Mean of tco usage data",round(np.mean(dram_tco_data),2), "Mean of optane data",round(np.mean(optane_tco_daa),2), "Mean of total data",round(np.mean(total_tco_data),2))

avg_dram_usage_data=running_average(dram_usage_data)
avg_optane_usage_data=running_average(optane_usage_data)
# avg_total_usage_data=running_average(total_usage_data)

save_array_to_file( f"{directory}/plots/raw/dram_usage_mb",dram_usage_data)
# save_array_to_file( f"{directory}/plots/raw/avg_dram_vmrss",avg_dram_usage_data)
save_array_to_file( f"{directory}/plots/raw/optane_usage_mb",optane_usage_data)
# save_array_to_file( f"{directory}/plots/raw/avg_optane_vmrss",avg_optane_usage_data)
# save_array_to_file( f"{directory}/plots/raw/total_vmrss",total_usage_data)
# save_array_to_file( f"{directory}/plots/raw/avg_total_vmrss",avg_total_usage_data)


if args.plot_pdf==0:
    print("Exiting without plotting")
    exit(0)
    
if not os.path.exists(f"{directory}/plots"):
    os.makedirs(f"{directory}/plots")
directory=f"{directory}/plots"

# define the tick formatter function
def comma_format(x, pos):
    return "{:,.0f}".format(x)

# apply the formatter to the y-axis ticks
formatter = ticker.FuncFormatter(comma_format)

ax=plt.gca()

dram_x_data = np.linspace(0, (len(dram_usage_data)-1)*trend_sleep_duration, num=len(dram_usage_data))
optane_x_data = np.linspace(0, (len(optane_usage_data)-1)*trend_sleep_duration, num=len(dram_usage_data))
# print(x_data)
plt.plot(dram_x_data, dram_usage_data, label="DRAM", color="blue")
plt.plot(optane_x_data, optane_usage_data, label="Optane", color="red")

plt.xlabel('Time in seconds',fontsize=lfont-4)
plt.ylabel('VmRSS in MB',fontsize=lfont-4)

plt.legend(fontsize=lfont-10)

ax.yaxis.set_major_formatter(formatter)
plt.grid()

format_axis(ax,lfont=lfont-6)

plt.savefig(f"{directory}/plot_numastat.png", dpi=300,bbox_inches="tight")

# ===============================
color_array=["blue","red","green","orange","purple","pink","brown"]
style_arr=["-","--","-.",":","-","--","-."]

# new figure., plot numa_node_data
fig, ax = plt.subplots()
# plot each numa node data
for i in range(1, len(numa_node_data)+1):
    # plot the data
    x_data = np.linspace(0, (len(numa_node_data[i])-1)*trend_sleep_duration, num=len(numa_node_data[i]))
    plt.plot(x_data, numa_node_data[i], label=f"Node {i-1}", color=color_array[i-1], linestyle=style_arr[i-1],linewidth=2)
plt.xlabel('Time in seconds',fontsize=lfont-4)
plt.ylabel('Size in MB',fontsize=lfont-4)
plt.legend(fontsize=lfont-10)
ax.yaxis.set_major_formatter(formatter)
plt.grid()
format_axis(ax,lfont=lfont-6)
if args.plot_pdf==1:
    print("Saving to ", f"{directory}/plot_numastat_numa_nodes.png")
    plt.savefig(f"{directory}/plot_numastat_numa_nodes.png", dpi=300,bbox_inches="tight")
elif args.plot_pdf==2:
    print("Saving to ", f"{directory}/plot_numastat_numa_nodes.pdf")
    plt.savefig(f"{directory}/plot_numastat_numa_nodes.pdf", dpi=300,bbox_inches="tight")
else:
    print("Invalid plot_pdf option. Exiting")
