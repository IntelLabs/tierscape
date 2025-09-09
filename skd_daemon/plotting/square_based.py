

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns
import matplotlib
import matplotlib.patches as mpatches
import sys
import itertools
import pdb
import numpy as np
import argparse, csv, pdb
import pandas as pd 
import random
import matplotlib.cm as cm
import os
from matplotlib.ticker import FormatStrFormatter
import argparse
import math

 
def fix_axis(ax):
    ax.yaxis.set_major_formatter(FormatStrFormatter('%.2f'))
#     ax.get_yaxis().set_major_formatter(
#     matplotlib.ticker.FuncFormatter(lambda x, p: format(int(x), ',')))
    
def fix_axis_int(ax):   
    ax.get_yaxis().set_major_formatter(
    matplotlib.ticker.FuncFormatter(lambda x, p: format(int(x), ',')))

# total arguments



# total arguments

parser = argparse.ArgumentParser(description='Process some integers.')
parser.add_argument('--input_data', '-i', type=str, required=True, help='Input data file path')
parser.add_argument('--heap_min', '-min', type=str, default="3ff03c3a7000", help='Minimum heap size')
parser.add_argument('--heap_max', '-max',type=str, default="ffffffffffff", help='Maximum heap size')
# parser.add_argument('--output_file', type=str, default=None, help='Output file path')
parser.add_argument('--marker_size', '-m', type=int, default=5, help='Output file path')

args = parser.parse_args()
input_data = args.input_data
output_file = os.path.dirname(args.input_data)
output_file=f"{output_file}/plots/pebs.png"
# args.output_file




reions_df=pd.read_csv(input_data,sep=",",header=None)
reions_df.columns = ['time', 'start','height','width','hotness']
# reions_df
# print(max(reions_df["time"]))
# print(max(reions_df["start"]))
# set(reions_df["hotness"])

address = [max(reions_df["start"])]
time = [max(reions_df["time"])]
  
# # Create the pandas DataFrame with column name is provided explicitly
df = pd.DataFrame([address,time]).T
df.columns = ['address', 'time']
df


# In[10]:
min_addr=np.min(reions_df["start"])

regions=list(reions_df.itertuples(index=False, name=None))


# In[11]:


r_len=len(regions)
cmap =sns.light_palette('#DD1A1A', input='rgb', as_cmap=True)
norm = matplotlib.colors.Normalize(vmin=min(reions_df["hotness"]), vmax=max(reions_df["hotness"]))



# In[12]:

import math
def roundup(x):
    return int(math.ceil(x / 100.0)) * 100

plt.figure(figsize=(20, 16), dpi=300)
lfont=18

plt.scatter(x=df.time,     y=df.address,s=0)
plt.xlabel("Time (ms) ",fontweight ='bold', fontsize=lfont)
plt.ylabel("Address space", fontweight ='bold',size=lfont)
for t in regions:
    left, bottom, height, width, hotness = t
    
        
    rect=mpatches.Rectangle((left,bottom),width,height
                            ,fill=True
                            ,alpha=.9
                            ,edgecolor=None
                            ,linewidth=2
                        #    ,facecolor="red"
                        ,facecolor=cmap(norm(hotness))
                           )
                        #    ,facecolor=cmap(norm(hotness)))
    plt.gca().add_patch(rect)
ax = plt.gca()
# fix_axis(ax)
# plt.ticklabel_format(style='sci', axis='y', scilimits=(0,0))
# ax.ticklabel_format(style='sci',scilimits=(-3,4),axis='both')

fmt = lambda x, pos: '{:.0f} MB'.format(roundup((x-min_addr)/1000000), pos)
ax.yaxis.set_major_formatter(matplotlib.ticker.FuncFormatter(fmt))


fmt = lambda x, pos: '{:.0f}'.format(((x)), pos)
ax.xaxis.set_major_formatter(matplotlib.ticker.FuncFormatter(fmt))

plt.grid()

ax.tick_params(axis='both', which='major', labelsize=lfont)
plt.tight_layout()
# 
if not output_file is None:
    plt.savefig(output_file)


