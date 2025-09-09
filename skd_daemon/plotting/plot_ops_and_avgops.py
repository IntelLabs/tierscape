


import matplotlib.pyplot as plt
import argparse
import os
import matplotlib.ticker as ticker

import sys
sys.path.append(".")
from plot_utils import *

# create the argument parser
parser = argparse.ArgumentParser(description="Read contents of input file and write to output file")
parser.add_argument('--input', '-i', metavar='input_file', dest='input_file', required=True, help='input file path')
parser.add_argument('--pdf', '-p', metavar='plot_pdf', dest='plot_pdf', default=0, required=False, help='Enable PDF saving',type=int)


args = parser.parse_args()

if args.input_file is None:
    print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
    exit(1)
else:
    print(f"=====================\nPlotting {os.path.basename(__file__)}")




def running_average(numbers):
    running_sum = 0
    averages = []
    for i, number in enumerate(numbers):
        running_sum += number
        averages.append(running_sum / (i + 1))
    return averages

ops=[]
avg_ops = []
do_we_have_avg_op=0
directory = os.path.dirname(args.input_file)
with open(args.input_file, "r") as f:
    for line in f:
        if "msec latency" in line:
            cols = line.split(',')
            # print(cols[1])
            poi_txt=cols[2]
            op_val = poi_txt.strip().split()[0]
            avg_ops_val = poi_txt.strip().split()[2][:-1]
            # print(op_val, avg_ops_val)
            ops.append(float(op_val))
            avg_ops.append(float(avg_ops_val))
            do_we_have_avg_op=1
        elif "current ops/sec; est completion in" in line:
            op_val = line.split()[6]
            ops.append(float(op_val))
        elif "Running time" in line:
            # print("Running time", line)
            op_val = line.split()[3]
            ops.append(float(op_val))
        elif "| Time(s)" in line:
            op_val = line.split()[7]
            ops.append(float(op_val))
        elif "Runtime" in line:
            op_val = line.split()[1]
            ops.append(float(op_val))
        else:
            pass
            # print(line)


if do_we_have_avg_op==0:
    # ycsb does not print running average. Need to calcualte it.
    avg_ops = running_average(ops)

# print last 10 element of ops


# if len of ops and av_ops is zero then return
if len(ops)==0 or len(avg_ops)==0:
    print("No OPS data found in the input file")
    exit(1)

if len(ops) > 11:
    percent_ops=(ops[-10:])
    # remove the last element from percent_ops
    percent_ops=percent_ops[:-1]
    percent_avg_ops = running_average(percent_ops)
else:
    percent_avg_ops = running_average(ops)

if not os.path.exists(f"{directory}/plots"):
    os.makedirs(f"{directory}/plots")
directory=f"{directory}/plots"
print(directory)

# define the tick formatter function
def comma_format(x, pos):
    return "{:,.0f}".format(x)

# apply the formatter to the y-axis ticks
formatter = ticker.FuncFormatter(comma_format)

ax=plt.gca()

# check if file f"{directory}/raw/ops" exists
if not os.path.exists(f"{directory}/raw/ops"):
    save_array_to_file(f"{directory}/raw/ops",ops)
else:
    ops= read_array_from_file(f"{directory}/raw/ops")

if not os.path.exists(f"{directory}/raw/avg_ops"):
    save_array_to_file(f"{directory}/raw/avg_ops",avg_ops)
else:
    avg_ops= read_array_from_file(f"{directory}/raw/avg_ops")    
# save_array_to_file(f"{directory}/raw/avg_ops",avg_ops)
save_array_to_file(f"{directory}/raw/percent_avg_ops",percent_avg_ops)

# ------------------------------------------------

if args.plot_pdf==0:
    print("Not plotting: ops")
    exit(0)
    
plt.plot(avg_ops, label="Average Ops/sec", color="k",zorder=20)
plt.plot(ops, label="Ops/sec", color="red",zorder=10)
plt.xlabel('Time (in seconds)')
plt.ylabel('Operations per second')
# plt.title('Average operations per second over time')       

if max(avg_ops) < 1000:
    ax.yaxis.set_major_formatter(ticker.FormatStrFormatter('%0.1f'))
else:
    ax.yaxis.set_major_formatter(formatter)
plt.legend()
# plt.grid()
plt.grid(True, which='both', linestyle='--', zorder=0)

if args.plot_pdf==2:
    print("Saving pdf to: ", directory + "/plot_ops_avgops.pdf")
    plt.savefig(f"{directory}/plot_ops_avgops.pdf", dpi=100,bbox_inches="tight")
elif args.plot_pdf==1:
    print("Saving plot to: ", directory + "/plot_ops_avgops.png")
    plt.savefig(f"{directory}/plot_ops_avgops.png", dpi=300,bbox_inches="tight")

# ------------------------------------------------
# # get a new plot
# plt.clf()
# plt.xlabel('Time (in seconds)')
# plt.ylabel('Average operations per second')
# plt.title('Average operations per second over time')       

# if max(ops) < 1000:
#     ax.yaxis.set_major_formatter(ticker.FormatStrFormatter('%0.1f'))
# else:
#     ax.yaxis.set_major_formatter(formatter)
# plt.legend()
# plt.grid()
# plt.savefig(f"{directory}/plot_rawops.png", dpi=300,bbox_inches="tight")
