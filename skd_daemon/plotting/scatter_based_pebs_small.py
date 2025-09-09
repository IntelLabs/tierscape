#!/usr/bin/env python3

import seaborn as sns
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
import matplotlib
import sys


# In[31]:


input_data=None
heap_min=None
heap_max=None
output_file=None


# total arguments
n = len(sys.argv)
if n !=5 and n!=3:
    print(f"Need CSV file and graph output file (extension can be specified), heap min, and heap max in hex format. Args given: {n}")
    exit()

if (n==5):

    input_data=sys.argv[1]
    output_file=sys.argv[2]
    heap_min=sys.argv[3]
    heap_max=sys.argv[4]
else:

    input_data=sys.argv[1]
    output_file=sys.argv[2]
    heap_min=hex(0)
    heap_max="FFFFFFFFFFFFFFFF"
    # heap_min="6DCF5AEF7000"
    # heap_max="E40B56C19000"
    # heap_min=hex(140217018589184)
    # heap_max=hex(140327018589184)



# input_data="data/skd_log"
# heap_min="0x7ff6c053f000"
# heap_max="0x7fffeadffa00"

# Region 0 address is 0x7fffaf453000 : 0x7fffeadffa00
# Region 1 address is 0x7ffcaf9f7000 : 0x7ffceb3a3a00
# Region 2 address is 0x7ff9b7f9b000 : 0x7ff9f3947a00
# Region 3 address is 0x7ff6c053f000 : 0x7ff6fbeeba00
# 

delim=","

xlabel = 'Samples'
ylabel = "Virtual Address"
plot_log = False
solid_color_index = [1]
solid_color = [None]
rotation_angle = 0
height_of_legends = 1.3
legendloc='upper left'



# dtypes = {'time': 'int', 'co': 'str', 'addr': 'str'}
# df = pd.read_csv(input_data,sep=delim, header=None,names=["time","cp", "addr"],on_bad_lines="warn",dtype=dtypes,skiprows=[0,1]).dropna()

dtypes = {'time': 'float', 'addr': 'long', 'hot':  'int'}
df = pd.read_csv(input_data,sep=delim, header=None,names=["time", "addr", "hot"],on_bad_lines="warn",dtype=dtypes,skiprows=[0,1]).dropna()
print(df.head())


# In[22]:


print(f"Total unqiue addresses captured {len(pd.unique(df['addr']))}")
idf = df

# b16 = lambda x: int(x,16)
# idf["addr"]=idf["addr"].apply(b16)
# print(idf.head())


# In[23]:


mdf = idf.sample(frac =1)
print(len(mdf))


# In[24]:


sdf=mdf.copy()

heap_min_int = int(heap_min,16)
heap_max_int = int(heap_max,16)
print(f"heap min {heap_min_int}")
print(f"heap max {heap_max_int}")
# # print()

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


import math
def roundup(x,val=1000.0):
    return np.max(0,int(math.ceil(x /val)) * val)

plt.figure(figsize=(20, 10), dpi=100)
lfont=18
sns.set_theme(style='white')
ax = plt.gca()

plt.grid()

# plt.scatter(pdf["time"], pdf["addr"], s=.1, alpha=0.6,  edgecolor=None, linewidth=1, cmap='Blues',c=pdf["counts"])
# plt.scatter(pdf["time"], pdf["addr"], s=.5, alpha=0.2,  edgecolor=None, linewidth=1, cmap='Blues', c=pdf["cp"])
plt.scatter(pdf["time"], pdf["addr"], s=2, alpha=1,  edgecolor="red", linewidth=1, c="red")
# plt.scatter(pdf["time"], pdf["addr"], s=.1, alpha=.5,  edgecolor=None, linewidth=None, c="red")

min_addr=np.min(pdf["addr"])

plt.xlabel("Time",fontsize=lfont)
plt.ylabel(ylabel,fontsize=lfont)


ax = plt.gca()


ax.tick_params(axis='both', which='major', labelsize=lfont)


# fmt = lambda x, pos: '{:.0f} MB'.format(roundup((x-min_addr)/1000000), pos)

fmt = lambda x, pos: '{:.0f} KB'.format(roundup(((x-min_addr)/1000),10), pos)

# ax.yaxis.set_major_formatter(matplotlib.ticker.FuncFormatter(fmt))


try:
    filename=output_file
    print(filename)
    plt.savefig(f"{filename}")
    # plt.tight_layout()
    # plt.show()
except Exception as e:
    print("Error in saving file", e)
    pass

