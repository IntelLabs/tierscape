

import seaborn as sns
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import matplotlib
import sys
import os
import argparse
import math
from matplotlib.ticker import FuncFormatter

import sys
sys.path.append(".")
from plot_utils import *

# total arguments

parser = argparse.ArgumentParser(description='Process some integers.')

parser.add_argument('--input_file', '-i', type=str, required=True, help='Input data file path')
parser.add_argument('--heap_min', '-min', type=str, default="7ff000000000", help='Minimum heap size')
parser.add_argument('--heap_max', '-max',type=str, default="ffffffffffff", help='Maximum heap size')
parser.add_argument('--marker_size', '-m', type=int, default=10, help='Output file path')
parser.add_argument('--pdf', '-p', metavar='plot_pdf', dest='plot_pdf', default=0, required=False, help='Enable PDF saving',type=int)
# add boolean argument named sampled


args = parser.parse_args()

if not os.path.exists(args.input_file) and not os.path.exists(args.input_file[:-3]):
	# print scriptname and input file missing
	print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
	exit(1)
else:
	print(f"=====================\nPlotting {os.path.basename(__file__)}")


    

# decompress input file using gunzip
is_compressed=0
if args.input_file.endswith(".gz"):
	is_compressed=1
	print(f"Decompressing {args.input_file} using gunzip")
	os.system(f"gunzip -f {args.input_file}")
	args.input_file = args.input_file[:-3]
	print(f"Decompressed file is {args.input_file}")
	# check if the file exists
	if not os.path.exists(args.input_file):
		# print scriptname and input file missing
		print(f"ERROR: {os.path.basename(__file__)}: Input file does not exist")
		exit(1)
  

if args.plot_pdf==0:
	print("PDF saving is disabled")

input_file = args.input_file
heap_min = args.heap_min
heap_max = args.heap_max

output_file = os.path.dirname(args.input_file)
# create plots if missing
if not os.path.exists(f"{output_file}/plots"):
	os.makedirs(f"{output_file}/plots")
output_file=f"{output_file}/plots/pebs.png"



delim=","

xlabel = 'Samples'

plot_log = False
solid_color_index = [1]
solid_color = [None]
rotation_angle = 0
height_of_legends = 1.3
legendloc='upper left'

print(input_file)



def read_raw_in_df(input_file):

	df = pd.read_csv(input_file, sep=":", header=None, names=["time", "addr"], on_bad_lines="warn")

	df['time'] = df['time'].astype(float)
	df['addr'] = df['addr'].str.strip().str.replace(" ", "").str.replace(":", "").str.replace("0x", "", regex=False)

	# addr contains hex entries
	# convert addr to hex
	# df['addr'] = df['addr'].apply(lambda x: int(x, 16))
	
	# drop where addr is 0
	df = df[df['addr'] != '0']

	# print("raw data -------------")
	# print(df.head())
	return df

def prune_out_of_range():
	global df
	df['addr'] = df['addr'].apply(lambda x: int(x, 16))
	print("Address len beore pruning", len(df))
 
	df = df.drop(df[df.addr < int(heap_min, 16)].index)
	df = df.drop(df[df.addr > int(heap_max, 16)].index)
 
	df['addr'] = df['addr'].apply(lambda x: hex(x))
 
	print("Address len after pruning", len(df))
	


def process_raw_in_df():
	global df
	prune_out_of_range()
	df['count'] = 1

  
	df['addr'] = df['addr'].apply(lambda x: int(x, 16) & ~0xFFF)
	df['time'] = df['time'].apply(lambda x: int(x // .2))
	df = df.groupby(['time', 'addr']).sum().reset_index()
	
	# print head
	
	# print("Processed data -------------")
	# print(df.head())
	# print(df.tail())
	# print("------------------")
	# # save df as csv
	# output_file = os.path.splitext(input_file)[0] + "_processed.csv"
	# df.to_csv(output_file, index=False)
 
	return df, output_file
 
# read the input file

 
df = read_raw_in_df(input_file)
df, processed_file = process_raw_in_df()


def roundup(x):
	return int(math.ceil(x / 100.0)) * 100


# dtypes = {'time': 'string', 'addr': 'string', 'count':  'float'}
# df = pd.read_csv(processed_file,sep=delim, header=None,names=["time", "addr", "hot"],on_bad_lines="warn",dtype=dtypes).dropna()

# convert the column to uint64 format
# df['time'] = df['time'].astype('float')
# df['addr'] = df['addr'].apply(lambda x: int(x, 16))
# print(df.head())

# sample df such that only 2000 entries remai
sampling_rate = len(df) // 100000
print(f"Sampling rate {sampling_rate}")
if sampling_rate > 1:	
	a_sdf = df.iloc[::sampling_rate, :]
else:
	a_sdf = df.iloc[::1, :]

print(f"Total unqiue addresses captured {len(pd.unique(a_sdf['addr']))}")
# print(f"Total unqiue pages captured {len(pd.unique(df['addr']))}")

# in a_sdf, eliminate point that will overlap and will add no info to the plot
a_sdf = a_sdf.drop_duplicates(subset=['time', 'addr'], keep='last')
print(f"PRUNED Total unqiue addresses captured {len(pd.unique(a_sdf['addr']))}")


lfont=24
sns.set_theme(style='white')
ax = plt.gca()

# cacluate mean ignoring outliers
outliers = a_sdf['count'].quantile(0.99)
# print len of outliers and toal elem
print(f"Outlier hotness: {outliers} Outliers: {len(a_sdf[a_sdf['count'] > outliers])} of {len(a_sdf)}")
# mean without outliers
mean = a_sdf[a_sdf['count'] < outliers]['count'].mean()
# std without outliers
std = a_sdf[a_sdf['count'] < outliers]['count'].std()

# # # Generate Gaussian distribution
# mu = a_sdf['count'].mean()  # Mean
# sigma = a_sdf['count'].std()  # Standard deviation

# print mean and std
print(f"Mean: {mean}, Std: {std}")

gaussian_values = np.random.normal(mean, std, a_sdf.shape[0])
# # Define the colors for the colormap
# from matplotlib.colors import LinearSegmentedColormap
# colors = ['white', "yellow", '#FF0000']  # White to red
# # Create the colormap
# cmap = LinearSegmentedColormap.from_list('white_to_red', colors)
# plt.scatter(a_sdf["time"], a_sdf["addr"], s=args.marker_size, alpha=.8,  edgecolor="k", linewidth=.1, c="blue")
# plt.scatter(a_sdf["time"], a_sdf["addr"], s=args.marker_size, alpha=.8,  edgecolor="k", linewidth=.4, c="k")

# new cmap orange to red
from matplotlib.colors import LinearSegmentedColormap
colors = ['orange', "yellow", '#FF0000']  # White to red
# Create the colormap
custom_cmap = LinearSegmentedColormap.from_list('white_to_red', colors)



plt.scatter(a_sdf["time"], a_sdf["addr"], s=args.marker_size, alpha=.4,  edgecolor="k", linewidth=.01,c="red")

# plt.scatter(a_sdf["time"], a_sdf["addr"], s=args.marker_size, alpha=.8,  edgecolor="k", linewidth=.1)
# plt.scatter(a_sdf["time"], a_sdf["addr"], s=args.marker_size, alpha=.8)



ax = plt.gca()
ax.tick_params(axis='both', which='major', labelsize=lfont-6)

min_page_addr=np.min(a_sdf["addr"])
# max_page_addr=np.max(a_sdf["addr"])
# # print address space size in GB
# total_pages = (max_page_addr-min_page_addr)
# size_in_gb = (total_pages*4096) / 1000000000

# print(min_page_addr, max_page_addr, total_pages, size_in_gb)
# # print in hex first two entries
# print(hex(min_page_addr), hex(max_page_addr))

# # ensure 5 ytiks
# yticks = np.linspace(min_page_addr, max_page_addr, 5)
# yticks = [int(x) for x in yticks]
# ax.set_yticks(yticks)

# fmt = lambda x, pos: '{:.0f} MB'.format(roundup((x-min_addr)/1000000), pos)
fmt = lambda x, pos: '{:.0f} GB'.format((abs(x-min_page_addr)/1000000000), pos)
ax.yaxis.set_major_formatter(matplotlib.ticker.FuncFormatter(fmt))



ylabel = "Address Space"
plt.xlabel("Time in seconds",fontsize=lfont)
plt.ylabel(ylabel,fontsize=lfont)
plt.grid(False)
format_axis(ax)
try:
	print(f"Saving plot to {output_file}")
	plt.savefig(output_file, dpi=100, bbox_inches='tight')

	if args.plot_pdf==2:
		
		output_file = os.path.dirname(args.input_file)
		output_file=f"{output_file}/plots/pebs.pdf"
		print("Saving PDF to", output_file)
		plt.savefig(output_file, dpi=100, bbox_inches='tight')
except Exception as e:
	print(f"Failed to save plot to {output_file}")
	print(e)


if is_compressed==1:
	print(f"Compressing {input_file} using gzip")
	os.system(f"gzip -f {input_file}")
	