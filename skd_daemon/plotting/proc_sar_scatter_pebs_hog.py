

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

parser.add_argument('--input_data', '-i', type=str, required=True, help='Input data file path')
parser.add_argument('--heap_min', '-min', type=str, default="fffff", help='Minimum heap size')
parser.add_argument('--heap_max', '-max',type=str, default="ffffffffffff", help='Maximum heap size')
# parser.add_argument('--output_file', type=str, default=None, help='Output file path')
parser.add_argument('--marker_size', '-m', type=int, default=1, help='Output file path')
parser.add_argument('--pdf', '-p', metavar='plot_pdf', dest='plot_pdf', default=0, required=False, help='Enable PDF saving',type=int)
# add boolean argument named sampled
parser.add_argument('--not_sampled', '-ns',  help='Disale sampling', type=bool, default=False, dest='not_sampled')

args = parser.parse_args()

input_data = args.input_data
heap_min = args.heap_min
heap_max = args.heap_max

output_file = os.path.dirname(args.input_data)
# create plots if missing
if not os.path.exists(f"{output_file}/plots"):
    os.makedirs(f"{output_file}/plots")
output_file=f"{output_file}/plots/pebs.png"
# args.output_file

heap_min_int = int(heap_min,16)
heap_max_int = int(heap_max,16)
# heap_max_int = heap_min_int + 1000000000
print(f"heap min {heap_min_int}")
print(f"heap max {heap_max_int}")
# # print()
print(output_file)

delim=","

xlabel = 'Samples'

plot_log = False
solid_color_index = [1]
solid_color = [None]
rotation_angle = 0
height_of_legends = 1.3
legendloc='upper left'

print(input_data)


dtypes = {'time': 'string', 'addr': 'string'}
df = pd.read_csv(input_data,sep=delim, header=None,names=["time", "addr"],on_bad_lines="warn",dtype=dtypes,skiprows=[0,1]).dropna()


# convert the column to numeric format and drop invalid rows
df['time'] = pd.to_numeric(df['time'], errors='coerce')
df['addr'] = pd.to_numeric(df['addr'], errors='coerce')
# add a column hot with all 1
df['hot'] = 1
# df['hot'] = pd.to_numeric(df['hot'], errors='coerce')
df = df.dropna()
print(df.head())
# df['hot'] = [3*x for x in df['hot']]

# convert the column to uint64 format
df['time'] = df['time'].astype('float')
df["addr"]=df["addr"].astype("uint64")


print(f"Total unqiue addresses captured {len(pd.unique(df['addr']))}")
idf = df
mdf = idf.sample(frac =1)
print(len(mdf))
sdf=mdf.copy()



sdf = sdf.drop(sdf[sdf.addr < heap_min_int].index)
sdf = sdf.drop(sdf[sdf.addr > heap_max_int].index)

print(f"data min value {min(sdf.addr)}")
print(f"data max value {max(sdf.addr)}")

# print(sdf["addr"].describe().apply(lambda x: format(x, 'f')))

# print(sdf.head())
print(len(sdf))
sdf
sdf.sort_values(by=['addr'],ascending=False)


# In[25]:


pdf=sdf.copy()

print(len(pdf))
print(f"Total unqiue addresses captured {len(pd.unique(pdf['addr']))}")
pdf['addr']=(pdf.addr.values>>12)
pdf['addr']=(pdf.addr.values<<12)


print(pdf.head())
print(len(pdf))
print(f"Total unqiue pages captured {len(pd.unique(pdf['addr']))}")



# sample evey 5th point from pdf
if args.not_sampled==True:
    print("Sampling data", args.not_sampled)
    a_sdf = pdf.iloc[::5, :]
else:
    print("Not Sampling data", args.not_sampled)
    a_sdf = pdf


def roundup(x):
    return int(math.ceil(x / 100.0)) * 100

# plt.figure(figsize=(20, 20), dpi=100)
lfont=24
sns.set_theme(style='white')
ax = plt.gca()


# Generate Gaussian distribution
mu = a_sdf['hot'].mean()  # Mean
sigma = a_sdf['hot'].std()  # Standard deviation
gaussian_values = np.random.normal(mu, sigma, a_sdf.shape[0])
# Define the colors for the colormap
from matplotlib.colors import LinearSegmentedColormap
colors = ['white', "yellow", '#FF0000']  # White to red
# Create the colormap
cmap = LinearSegmentedColormap.from_list('white_to_red', colors)




# plt.scatter(a_sdf["time"], a_sdf["addr"], s=args.marker_size, alpha=.8,  edgecolor="k", linewidth=.1, c="blue")

plt.scatter(a_sdf["time"], a_sdf["addr"], s=args.marker_size, alpha=.8,  edgecolor="k", linewidth=.1, c=gaussian_values, cmap="YlOrRd")
# plt.scatter(a_sdf["time"], a_sdf["addr"], s=args.marker_size, alpha=.8,  edgecolor="k", linewidth=.4, c="k")



min_addr=np.min(a_sdf["addr"])
print(np.min(a_sdf["time"]), np.max(a_sdf["time"]))



ax = plt.gca()
ax.tick_params(axis='both', which='major', labelsize=lfont-6)


# # fmt = lambda x, pos: 'x={:f}\npos={:f}'.format(x, pos)
fmt = lambda x, pos: '{:.0f} MB'.format(roundup((x-min_addr)/1000000), pos)
fmt = lambda x, pos: '{:.0f} GB'.format(((x-min_addr)/1000000000), pos)
ylabel = "Address Space"
# # fmt = lambda x, pos: 'x={:f}'.format(x, pos)

# fmt = lambda x, pos: f'{int(x):X}'  # X means hexadecimal representation in uppercase
ax.yaxis.set_major_formatter(matplotlib.ticker.FuncFormatter(fmt))

# def hex_format(x, pos):
#     return f'{x:X}'  # X means hexadecimal representation in uppercase
# # Apply the custom formatter to the y-axis
# ax.yaxis.set_major_formatter(FuncFormatter(hex_format))

plt.xlabel("Time in seconds",fontsize=lfont)
plt.ylabel(ylabel,fontsize=lfont)
plt.grid(False)
format_axis(ax)
try:
    output_file = os.path.dirname(args.input_data)
    if args.plot_pdf==1:
        plt.savefig(f"{output_file}/plots/pebs.png", dpi=100, bbox_inches='tight')
    elif args.plot_pdf==2:
        plt.savefig(f"{output_file}/plots/pebs.pdf", dpi=100, bbox_inches='tight')
    else:
        pass
except Exception as e:
    print(f"Failed to save plot to {output_file}")
    print(e)


# # ================== Guassian plot ======================
# plt.figure(figsize=(20, 20), dpi=100)
# lfont=18
# sns.set_theme(style='white')
# ax = plt.gca()

# df = a_sdf
# output_file = os.path.dirname(args.input_data)
# output_file=f"{output_file}/plots/gauss.png"
# # Calculate mean and standard deviation
# mu = df['hot'].mean()
# sigma = df['hot'].std()

# df = df[df['hot'] != 0]


# # Generate the Gaussian curve
# x = np.linspace(df['hot'].min(), df['hot'].max(), 100)
# y = np.exp(-(x - mu)**2 / (2 * sigma**2)) / (sigma * np.sqrt(2 * np.pi))

# # Create the histogram
# plt.hist(df['hot'], bins=10, density=True, alpha=0.5)

# # Plot the Gaussian curve
# plt.plot(x, y, color='red', linewidth=2)

# # Set labels and title
# plt.xlabel('Values')
# plt.ylabel('Density')
# plt.title('Gaussian Distribution for DataFrame HOT')

# plt.grid()
# try:
#     print(f"Saving plot to {output_file}")
#     plt.savefig(output_file, dpi=300, bbox_inches='tight')
# except:
#     pass


# # ================== Histogram plot ======================
# plt.figure(figsize=(20, 10), dpi=100)
# output_file = os.path.dirname(args.input_data)
# output_file=f"{output_file}/plots/hist.png"

# # Define colormap from blue to red
# cmap = plt.cm.get_cmap('coolwarm')

# n, bins, patches = plt.hist(df['hot'], bins=100)  # Replace 'Column1' with the actual column name

# # # Set colormap for patches
# # for patch in patches:
# #     patch.set_facecolor(cmap(patch.get_height() / max(n)))


# plt.xlabel('Accesses',fontsize=lfont)
# plt.ylabel('Page Counts',fontsize=lfont)
# # plt.title('Histogram of Column1')
# plt.grid()
# plt.yscale('log')  # Set y-axis scale to logarithmic
# try:
#     print(f"Saving plot to {output_file}")
#     plt.savefig(output_file, dpi=300, bbox_inches='tight')
# except:
#     pass

