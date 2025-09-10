


import matplotlib.pyplot as plt
import argparse
import os
import matplotlib.ticker as ticker
import numpy as np

# add a library path
import sys
sys.path.append(".")
from plot_utils import *
from tier_config_simple import get_complete_uncompressed_tiers_info


# create the argument parser
parser = argparse.ArgumentParser(description="Read contents of input file and write to output file")
parser.add_argument('--input', '-i', metavar='input_file', dest='input_file', required=True, help='input file path')
parser.add_argument('--pdf', '-p', metavar='plot_pdf', dest='plot_pdf', default=0, required=False, help='Enable PDF saving',type=int)
parser.add_argument('--tsleep', '-ts', metavar='trend_sleep_duration', dest='trend_sleep_duration', default=2, required=False, type=int)

args = parser.parse_args()

trend_sleep_duration = args.trend_sleep_duration

# Load uncompressed tier configuration from shared config
try:
    uncompressed_tiers = get_complete_uncompressed_tiers_info()
    print(f"Loaded uncompressed tiers config: {uncompressed_tiers}")

    # Get configured tier IDs for filtering
    configured_tier_ids = [tier['virt_id'] for tier in uncompressed_tiers]
    fast_tier_idxes = [tier['virt_id'] for tier in uncompressed_tiers if tier['mem_type'] == 'DRAM']
    slow_tier_idxes = [tier['virt_id'] for tier in uncompressed_tiers if tier['mem_type'] == 'OPTANE']

    # Create tier mapping for display
    tier_mapping = {}
    for tier in uncompressed_tiers:
        tier_mapping[tier['virt_id']] = f"{tier['virt_id_name']} ({tier['mem_type']})"

    print(f"Configured tier IDs: {configured_tier_ids}")
    print(f"Fast tier indices (DRAM): {fast_tier_idxes}")
    print(f"Slow tier indices (OPTANE): {slow_tier_idxes}")
    print("Tier mapping:", tier_mapping)
except Exception as e:
    print(f"Warning: Failed to load tier config: {e}")
    # fatal error
    print("Error: Could not determine DRAM/OPTANE tier indices from config. Exiting.")
    exit(1)

# if input file is not provided, exit
if args.input_file is None:
    print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
    exit(1)
else:
    print(f"=====================\nPlotting {os.path.basename(__file__)}")

dram_usage_data=[]
optane_usage_data=[]
total_usage_data=[]

# Store data for each configured tier
tier_data = {tier_id: [] for tier_id in configured_tier_ids}

directory = os.path.dirname(args.input_file)
with open(args.input_file, "r") as f:
    for line in f:
        if "Total" in line:
            # replace multiple consecutive spaces with a single space
            line = ' '.join(line.split())
            cols = line.split(' ')
            
            try:
                # Collect data for configured tiers only
                dram_data = 0
                optane_data = 0
                
                for tier_id in configured_tier_ids:
                    tier_value = int(cols[tier_id+1])
                    tier_data[tier_id].append(tier_value)
                    
                    # Accumulate by memory type
                    if tier_id in fast_tier_idxes:
                        dram_data += tier_value
                    elif tier_id in slow_tier_idxes:
                        optane_data += tier_value

                total_data = dram_data + optane_data
                
            except Exception as e:
                print(f"Error parsing line: {line} {cols} with error {e}")
                # print stack trace
                import traceback
                traceback.print_exc()
                exit(1)
           
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

# # apply the formatter to the y-axis ticks
formatter = ticker.FuncFormatter(comma_format)

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
# plt.grid()

# format_axis(ax,lfont=lfont-6)

# # plt.savefig(f"{directory}/plot_numastat.png", dpi=300,bbox_inches="tight")

# ===============================
color_array=["blue","red","green","orange","purple","pink","brown"]
style_arr=["-","--","-.",":","-","--","-."]

# new figure., plot configured tier data
fig, ax = plt.subplots()
# plot each configured tier data
for i, tier_id in enumerate(sorted(configured_tier_ids)):
    tier_label = f"Node {tier_id}"
    x_data = np.linspace(0, (len(tier_data[tier_id])-1)*trend_sleep_duration, num=len(tier_data[tier_id]))
    plt.plot(x_data, tier_data[tier_id], label=tier_label, 
             color=color_array[i % len(color_array)], 
             linestyle=style_arr[i % len(style_arr)], linewidth=2)

plt.xlabel('Time in seconds',fontsize=lfont-4)
plt.ylabel('Size in MB',fontsize=lfont-4)
plt.legend(fontsize=lfont-10)
ax.yaxis.set_major_formatter(formatter)
plt.grid()
format_axis(ax,lfont=lfont-6)
if args.plot_pdf==1:
    print("Saving to ", f"{directory}/plot_numastat_configured_tiers.png")
    plt.savefig(f"{directory}/plot_numastat_configured_tiers.png", dpi=300,bbox_inches="tight")
elif args.plot_pdf==2:
    print("Saving to ", f"{directory}/plot_numastat_configured_tiers.pdf")
    plt.savefig(f"{directory}/plot_numastat_configured_tiers.pdf", dpi=300,bbox_inches="tight")
else:
    print("Invalid plot_pdf option. Exiting")
