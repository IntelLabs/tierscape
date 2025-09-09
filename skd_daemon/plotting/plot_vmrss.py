


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

args = parser.parse_args()

# if input file is not provided, exit
if args.input_file is None:
    print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
    exit(1)

plot_data=[]

directory = os.path.dirname(args.input_file)
with open(args.input_file, "r") as f:
    for line in f:
        if "VmRSS" in line:
            
            cols = line.split('\t')[1].split()[0]
            # print(cols)
            poi_txt=cols
            op_val = poi_txt.strip()
            # print(op_val, avg_ops_val)
            plot_data.append(int(op_val))

dram_data=np.array(plot_data)
tco_data=(dram_data/(1024*1024))*3

avg_plot_data=running_average(plot_data)

save_array_to_file( f"{directory}/plots/raw/vmrss",plot_data)
save_array_to_file( f"{directory}/plots/raw/avg_vmrss",avg_plot_data)
save_array_to_file( f"{directory}/plots/raw/vmrss_tco",tco_data)

if not os.path.exists(f"{directory}/plots"):
    os.makedirs(f"{directory}/plots")
directory=f"{directory}/plots"

# define the tick formatter function
def comma_format(x, pos):
    return "{:,.0f}".format(x)

# apply the formatter to the y-axis ticks
formatter = ticker.FuncFormatter(comma_format)

ax=plt.gca()

x_data = np.linspace(0, (len(plot_data)-1)*10, num=len(plot_data))
# print(x_data)
plt.plot(x_data, plot_data, label="VmRSS", color="red")


plt.xlabel('Time in seconds')
plt.ylabel('VmRSS in KB')

ax.yaxis.set_major_formatter(formatter)
plt.legend().remove()
plt.grid()
plt.savefig(f"{directory}/plot_vmrss.png", dpi=300,bbox_inches="tight")
