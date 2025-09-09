

import argparse
import matplotlib.pyplot as plt
import numpy as np
import os
import math
import pandas as pd
import matplotlib.ticker as mticker

import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import matplotlib.ticker as ticker


# add a library path
import sys
sys.path.append(".")
from plot_utils import *

def format_ticks(value, pos):
    if value >= 1000:
        value /= 1000
        # return f"{value:.0f}K"
        # return command separated value
        return "{:,.0f}K".format(value)
    
    return f"{value:.0f}"

# define the tick formatter function
def comma_format(x, pos):
    return "{:,.2f}".format(x)

# Define the number of shades
num_shades = 8

# Create a grayscale color map
cmap = mcolors.LinearSegmentedColormap.from_list(
    'grayscale', [(i/num_shades, i/num_shades, i/num_shades) for i in range(num_shades)])
# Define a list of markers for each line
markers = ['o', 's', 'd', '^', '*', 'x', 'p', 'h']

# Define the colors of the colormap
colors = [(1.0, 0.0, 0.0), (1.0, 0.5, 0.0), (1.0, 1.0, 0.0),
          (0.0, 1.0, 0.0), (0.0, 1.0, 1.0), (0.0, 0.0, 1.0),
          (0.5, 0.0, 1.0), (1.0, 0.0, 1.0)]

# Create the colormap using the LinearSegmentedColormap class
cmap_rb = mcolors.LinearSegmentedColormap.from_list('red_blue_colormap', colors, N=num_shades)

# ============================
num_shades = 8

# Create a list of RGBA values that goes from red to blue with varying shades
colors = [(1.0, 0.0, 0.0, i/num_shades) for i in range(num_shades)] + \
         [(0.0, 0.0, 1.0, i/num_shades) for i in range(num_shades)]

# Create the colormap using the ListedColormap class
cmap_rgb2 = mcolors.ListedColormap(colors, N=num_shades*2)

# ============================
# create the argument parser
parser = argparse.ArgumentParser(description="Read contents of input file and write to output file")
# add the input file argument
parser.add_argument('--input', '-i', metavar='input_file', dest='input_file', required=True, help='input file path')
# add the output file argument
parser.add_argument('--stats', '-s', metavar='stats', dest='stats', required=True, help='stats to plot: \'nr_c_size\', \'nr_pages\', \'faults\'')
parser.add_argument('--plot_size', '-g', metavar='plot_size', dest='plot_size', required=False, help='Limit the y-axis to the specified value', default=0                    ,type=int)
parser.add_argument('--limit', '-l', metavar='limit', dest='limit', required=False, help='Limit the y-axis to the specified value', default=0                    ,type=int)
parser.add_argument('--plot_avg', '-a', metavar='plot_avg', dest='plot_avg', required=False, help='Limit the y-axis to the specified value', default=0                    ,type=int)
parser.add_argument('--plot_log', '-lg', metavar='plot_log', dest='plot_log', required=False, help='Plot the last tier or not', default=1                    ,type=int)
parser.add_argument('--pdf', '-p', metavar='plot_pdf', dest='plot_pdf', default=0, required=False, help='Enable PDF saving',type=int)


# parse the arguments
args = parser.parse_args()
stats = args.stats
directory = os.path.dirname(args.input_file)
print("args.plot_avg",args.plot_avg)
# -------------------------------------

# define the stats to extract
available_stats = ['nr_c_size', 'nr_pages', 'faults']
stats_to_extract = available_stats
pool_config = ['BS', 'type', 'comp']

# read the input file and extract the stats for each tier
tier_stats = {}

tier_configs = read_array_from_pickle(f"{directory}/plots/raw/tier/tier_configs")
max_tier_id = int(read_array_from_file(f"{directory}/plots/raw/tier/max_tier_id")[0])
min_tier_id = int(read_array_from_file(f"{directory}/plots/raw/tier/min_tier_id")[0])

print(max_tier_id, min_tier_id)

if args.plot_pdf==0:
    print("Not plotting")
    exit(0)

# --------------------------------------
# get linestyle, memory pool and the compressor
def get_ls_m_c(tier_config):
    ls = tier_config[0]
    m = tier_config[1]
    c = tier_config[2]
    ret=[]
    if(c == 'lz4'):
        ret.append("solid")
    else:
        ret.append("dashed")
    
    if(m == 'zbud'):
        ret.append("o")
    else:
        ret.append("*")
    
    if (ls == '0'):
        ret.append("black")
    else:
        ret.append("blue")
    

        
    return ret

running_gmean=-1;




# -----------------------------------------------------
# plot the line plots for each tier
fig, ax = plt.subplots(figsize=(6, 6))
colors = ['blue', 'green', 'red', 'cyan', 'magenta', 'yellow', 'black', 'orange']
width = 0.2
# generate x positions array
num_bars = 8
# generate x positions for the bars
bar_width = 0.8 / num_bars




stacked_df=None
# cmap = plt.cm.get_cmap('coolwarm_r')
cmap = matplotlib.colormaps.get_cmap('coolwarm_r')

idx=0
label=["CTier 1", "CTier 2"]
label=["CT-1", "CT-2"]
for tier_id in range(min_tier_id, max_tier_id+1):

    # label = f'Tier {tier_id}'
    # print(stats, tier_id, tier_configs[tier_id])
    # plot_data=plot_data[:min_len]
    if stats=="faults_per_page":
        plot_data = read_array_from_file(f"{directory}/plots/raw/tier/tier_{tier_id}/faults")
        plot_data2 = read_array_from_file(f"{directory}/plots/raw/tier/tier_{tier_id}/nr_pages")
        plot_data = [plot_data[i]/plot_data2[i] if plot_data2[i] > 0 else 0 for i in range(len(plot_data))]
        # limit to 20
        plot_data = [x if x < 2 else 2 for x in plot_data]
    else:
        print("reading data from ",f"{directory}/plots/raw/tier/tier_{tier_id}/{stats}")
        plot_data =read_array_from_file(f"{directory}/plots/raw/tier/tier_{tier_id}/{stats}")
    
    color = colors[tier_id % len(colors)]
    val=get_ls_m_c(tier_configs[tier_id])
    
    if args.plot_avg==1:
        plot_data = running_average(plot_data)
    
    # get cdf data for plot_data
    if stats=="faults":
        plot_data = np.cumsum(plot_data)
        # yl_txt
    
    
    
    # print(plot_data)
    if stats == "nr_pages" and args.plot_size==1:
        # convert pages to gb
        plot_data = [(x * 4)/(1024*1024) for x in plot_data] 
    if stats == "nr_c_size":
        # Bytes to KB
        plot_data = [x / 1024 for x in plot_data] 

    x_time=range(len(plot_data))
    print(tier_id, len(plot_data), "x_time",len(x_time), x_time)
    x_time=[i*10 for i in x_time]

    # print(label)
    # print(plot_data)
    ax.plot( x_time, plot_data, label=label, linestyle=val[0], marker=val[1], color=val[2], markersize=4, linewidth=3,markevery=5)
    # ax.plot(x_time, plot_data, label=label, linestyle=val[0], marker=val[1], c=cmap(idx / (len(tier_stats.items())
    #  - 1)), markersize=4, linewidth=3,markevery=5)
    idx+=1
    # print(len(plot_data))
    if(stacked_df is None):
        stacked_df=pd.DataFrame([plot_data])
    else:
        new_df = pd.DataFrame([plot_data], columns=stacked_df.columns)
        stacked_df = pd.concat([stacked_df, new_df], ignore_index=True)

    if (args.plot_log==1):
        plt.yscale('log')
    
    # i=i+1

    if(running_gmean==-1):
        running_gmean=geometric_mean(plot_data)
    else:
        try:
            running_gmean= geo_mean([running_gmean, geometric_mean(plot_data)])
        except:
            print("ERROR: error while calculating gmean")
            running_gmean=0

            
if args.limit!=0:
    running_gmean=max(running_gmean,args.limit)
    print("limiting to ",running_gmean, "as" ,args.limit)
    ax.set_ylim(0, running_gmean)

if stats=="faults_per_page":
    ax.set_ylim(0, 2)

# ax.legend()
# legend_txt_arr = np.array([text.get_text() for text in plt.gca().get_legend().get_texts()])
ax.legend(label, loc='upper center', bbox_to_anchor=(0.4, 1), ncol=5, fontsize=lfont-6)

format_axis(ax,lfont=lfont-2)
fix_axis_int(ax)



# Apply the custom formatter to the y-axis ticks
# formatter = ticker.FuncFormatter(comma_format)
formatter = mticker.FuncFormatter(format_ticks)

# if not nr_c_size
if stats=="faults_per_page":
    # format float yaxis ticks
    ax.yaxis.set_major_formatter(ticker.FormatStrFormatter('%0.2f'))
elif stats != "nr_c_size":
    plt.gca().yaxis.set_major_formatter(formatter)
    # format ticks with comma for thousand separtor
    # ax.yaxis.set_major_formatter(ticker.FuncFormatter(lambda x, _: '{:,.0f}'.format(x)))
    
    
# print xticks
#  print xlim
print(plt.xlim())
print(plt.xticks())

plt.locator_params(axis='x', nbins=6)
print(plt.xticks())
# arr = np.arange(0, 10, 1.5)
# pick first 6
# arr = arr[:6]
# plt.xticks(plt.xticks()[0], ['0', '100', '200', '300', '400', '500'])  # Example custom labels
    

ax.grid(visible=True, axis='y',color="red",alpha=.2,linestyle="--")
filename_to_save=stats
# make first letter if stats capital
if stats == "nr_c_size":
    yl_txt = 'Compressed Data (KB)'
elif "nr_pages" in stats:
    yl_txt = '# Pages'
    if args.plot_size==1:
        yl_txt = 'Size (GB)'
        filename_to_save="nr_pages_size"
elif stats=="faults":
    # yl_txt = '# Faults'
    yl_txt = '# Faults Cummulative'
    
elif stats == "faults_per_page":
    yl_txt = 'Faults per page'
else:
    print("Its\'",stats,"\'")
    # yl_txt=stats.capitalize()
    yl_txt="Unknown '"+stats+"'"

# get ylim
ylim = ax.get_ylim()[1]
single_tick_range = ylim/10
ylim = (math.ceil(ylim/single_tick_range)+1)*single_tick_range
# set ylim
ax.set_ylim(0, ylim)

ax.set_ylabel(yl_txt+["_log" if args.plot_log==1 else ""][0]+["_avg" if args.plot_avg==1 else ""][0]
              , fontsize=lfont)


ax.set_xlabel('Time in seconds', fontsize=lfont)
print(f"--->Plot saved to plot_tier_lvl_{filename_to_save}.png")
if not os.path.exists(f"{directory}/plots"):
    os.makedirs(f"{directory}/plots")

if(args.plot_pdf==1):    
    plt.savefig(f"{directory}/plots/plot_tier_lvl_{filename_to_save}.png", dpi=300,bbox_inches="tight")
elif(args.plot_pdf==2):
    plt.savefig(f"{directory}/plots/plot_tier_lvl_{filename_to_save}.pdf", dpi=100,bbox_inches="tight")
else:
    print("Not plotting hotness")

