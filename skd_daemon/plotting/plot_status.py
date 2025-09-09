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
# add metric argument
parser.add_argument('--metric', '-m', metavar='metric', dest='metric', required=True, help='metric to plot')
parser.add_argument('--pdf', '-p', metavar='plot_pdf', dest='plot_pdf', default=1, required=False, help='Enable PDF saving',type=int)
parser.add_argument('--tsleep', '-ts', metavar='trend_sleep_duration', dest='trend_sleep_duration', default=2, required=False, type=int)

args = parser.parse_args()

trend_sleep_duration = args.trend_sleep_duration

if args.input_file is None:
    print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
    exit(1)
else:
    print(f"=====================\nPlotting {os.path.basename(__file__)}")


if args.plot_pdf==0:
    print("Not Plotting")
    exit(0)

# read the input file extract all the lines containined metric and use the second to create an arr
metric_data=[]
directory = os.path.dirname(args.input_file)
with open(args.input_file, "r") as f:
	for line in f:
		if f"{args.metric}" in line:
			# replace multiple consecutive spaces with a single space
			line = ' '.join(line.split())
			cols = line.split(' ')
			metric_data.append(int(cols[1]))
			# print(line)

# if metric_data len is 0, error and exit
if len(metric_data) == 0:
    print(f"ERROR: {os.path.basename(__file__)}: No data found for metric '{args.metric}'")
    exit(1)

# offset metric_data by the first entry
metric_data = np.array(metric_data)
# metric_data = metric_data - metric_data[0]

x_arr = np.arange(0, len(metric_data))
x_arr = x_arr * trend_sleep_duration

# create an array with 10 20 30 same length as meetric-data
# x = np.arange(0, len(metric_data)*trend_sleep_duration, trend_sleep_duration)
# plot metric_data and save as png in plots sir with metric_data in name
fig, ax = plt.subplots()
ax.plot(x_arr, metric_data, linestyle='-',  color='k', linewidth=2,
        label=args.metric)
ax.set(xlabel='Time (s)', ylabel=args.metric)

ax.grid()


# axis in numbers command format
ax.yaxis.set_major_formatter(ticker.FuncFormatter(lambda x, pos: '{:,.0f}'.format(x)))

format_axis(ax, lfont-10)
plt.tight_layout()

if args.plot_pdf==1:
    # fit the plot to the figure
    print("Saving plot to: ", directory + "/plots/status_" + args.metric + ".png")
    plt.savefig(directory + "/plots/status_" + args.metric + ".png")
elif args.plot_pdf==2:
    print("Saving plot to: ", directory + "/plots/status_" + args.metric + ".pdf")
    plt.savefig(directory + "/plots/status_" + args.metric + ".pdf")
else:
    print(f"Not saving plot as args.plot_pdf is {args.plot_pdf}")

# print(metric_data)