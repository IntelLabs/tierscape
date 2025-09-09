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
parser.add_argument('--dir', '-d', metavar='plot_dir', dest='plot_dir', required=True, help='input file path')
parser.add_argument('--tsleep', '-ts', metavar='trend_sleep_duration', dest='trend_sleep_duration', default=2, required=False, type=int)
parser.add_argument('--pdf', '-p', metavar='plot_pdf', dest='plot_pdf', default=1, required=False, help='Enable PDF saving',type=int)



print(f"=====================\nPlotting {os.path.basename(__file__)}")

args = parser.parse_args()
trend_sleep_duration = args.trend_sleep_duration



tco_files = []
allowed_tco_files = ['dram_usage_mb', 'optane_usage_mb', 'zswap_dram_usage_mb', 'zswap_optane_usage_mb']
for root, dirs, files in os.walk(args.plot_dir):
    for file in files:
        if file in allowed_tco_files:
            tco_files.append(os.path.join(root, file))



optane_usage_mb=[]
dram_usage_mb=[]

raw_data={}
for tcf in tco_files:
    filename=os.path.basename(tcf)
    arr=read_array_from_file(tcf)
    raw_data[filename]=arr
    
min_len=0
for k in raw_data.keys():
    if min_len==0:
        min_len=len(raw_data[k])
    else:
        if len(raw_data[k]) < min_len:
            min_len=len(raw_data[k])

# trim raw_data to min_len
for k in raw_data.keys():
    if len(raw_data[k]) > min_len:
        print("Trimming {} from {} to {}".format(k, len(raw_data[k]), min_len))
        raw_data[k]=raw_data[k][:min_len]


# ========================================================================


def function_plot_data(df, to_plot, plot_avg=True, statname=None,label_sep_tco=None,ylabel=None):
    x_data = np.linspace(0, (len(dram_tco)-1)*trend_sleep_duration, num=len(dram_tco))
    ax=plt.gca()
    fig, ax = plt.subplots()
    plt.grid()
    base=None
    zorder=100

    for i in to_plot:
        zorder-=1
        if base is None:
            ax.fill_between(x_data, df[i], cmap="gist_yarg",zorder=zorder)
            base = df[i]
        else:
            y_data=np.array(base)+np.array(df[i])
            ax.fill_between(x_data, base+df[i],   cmap="gist_yarg",zorder=zorder)
            base = base+df[i]
        
    total_tco = 0
    for i in to_plot:
        total_tco += df[i]
    running_average_tco = running_average(total_tco)
    plt.plot(x_data, running_average_tco, color="black",zorder=101,linestyle="--")
    label_sep_tco.append("Avg. TCO")

    ax.legend(label_sep_tco, loc='upper center',bbox_to_anchor=(.5,1.2),  ncol=3, fancybox=True, shadow=False, fontsize=lfont-10)

    plt.xlabel('Time in seconds',fontsize=lfont-4)
    if not ylabel is None:
        plt.ylabel(ylabel,fontsize=lfont-4)

    format_axis(ax,lfont=lfont-6)

    if args.plot_pdf==1:
        print("plot_tco: Saving Png to ", f"{args.plot_dir}/plot_stacked_{statname}.png")
        plt.savefig(f"{args.plot_dir}/plot_stacked_{statname}.png", dpi=300,bbox_inches="tight")
    else:
        print("plot_tco: Saving PDF to ", f"{args.plot_dir}/plot_stacked_{statname}.pdf")
        plt.savefig(f"{args.plot_dir}/plot_stacked_{statname}.pdf", dpi=100,bbox_inches="tight")

# ==================================
dram_usage = raw_data['dram_usage_mb']
zswap_dram_usage_mb = raw_data['zswap_dram_usage_mb']
optane_usage = raw_data['optane_usage_mb']
zswap_optane_usage_mb = raw_data['zswap_optane_usage_mb']

total_dram_usage_mb= [x+y for x,y in zip(raw_data['dram_usage_mb'], raw_data['zswap_dram_usage_mb'])]
total_optane_usage_mb= [x+y for x,y in zip(raw_data['optane_usage_mb'], raw_data['zswap_optane_usage_mb'])]

# =========================
total_dram_tco=[3*x for x in total_dram_usage_mb]
total_optane_tco=total_optane_usage_mb
max_total_tco_val=max(np.max(total_dram_tco), np.max(total_optane_tco))
total_dram_tco=(np.array(total_dram_tco)/max_total_tco_val)*100
total_optane_tco=(np.array(total_optane_tco)/max_total_tco_val)*100
# ==================
dram_tco = [3*x for x in dram_usage]
zswap_dram_tco = [3*x for x in zswap_dram_usage_mb]
optane_tco = [x for x in optane_usage]
zswap_optane_tco = [x for x in zswap_optane_usage_mb]

max_tco_val=max(np.max(dram_tco), np.max(zswap_dram_tco), np.max(optane_tco), np.max(zswap_optane_tco))
dram_tco=(np.array(dram_tco)/max_tco_val)*100
zswap_dram_tco=(np.array(zswap_dram_tco)/max_tco_val)*100
optane_tco=(np.array(optane_tco)/max_tco_val)*100
zswap_optane_tco=(np.array(zswap_optane_tco)/max_tco_val)*100

# ==============
df_tco = pd.DataFrame({'dram_tco': dram_tco, 'zswap_dram_tco': zswap_dram_tco, 'optane_tco': optane_tco, 'zswap_optane_tco': zswap_optane_tco})
label_sep_tco=['DRAM', 'Zswap DRAM', 'Optane', 'Zswap Optane']
to_plot=["dram_tco", "zswap_dram_tco", "optane_tco", "zswap_optane_tco"]
function_plot_data(df_tco, to_plot, plot_avg=True,statname="tco_sep",label_sep_tco=label_sep_tco,ylabel="'TCO % relative to All-DRAM'")

# =======================
# df = pd.DataFrame({'total_dram_tco': total_dram_tco, 'total_optane_tco': total_optane_tco})
# label_sep_tco=['Total DRAM', 'Total NVMM']
# to_plot=["total_dram_tco", "total_optane_tco"]
# function_plot_data(df, to_plot, plot_avg=True,statname="tco",label_sep_tco=label_sep_tco,ylabel="'TCO % relative to All-DRAM'")

# ======= plot usage
df_usage = pd.DataFrame({'dram_usage': dram_usage, 'zswap_dram_usage_mb': zswap_dram_usage_mb, 'optane_usage': optane_usage, 'zswap_optane_usage_mb': zswap_optane_usage_mb})
label_sep_usage=['DRAM', 'Zswap DRAM', 'Optane', 'Zswap Optane']
to_plot=["dram_usage", "zswap_dram_usage_mb", "optane_usage", "zswap_optane_usage_mb"]
function_plot_data(df_usage, to_plot, plot_avg=True,statname="usage",label_sep_tco=label_sep_usage, ylabel="All usage in MB")

# ==========

df_zswap = pd.DataFrame({'zswap_dram_usage_mb': zswap_dram_usage_mb, 'zswap_optane_usage_mb': zswap_optane_usage_mb})
label_sep_zswap=['Zswap DRAM', 'Zswap Optane']
to_plot=["zswap_dram_usage_mb", "zswap_optane_usage_mb"]
function_plot_data(df_zswap, to_plot, plot_avg=True,statname="zswap_usage",label_sep_tco=label_sep_zswap, ylabel="Zswap usage in MB")