import math

import seaborn as sns
import itertools
import pandas as pd
from sklearn import preprocessing
import matplotlib.pyplot as plt
import pdb
import numpy as np
import matplotlib
import matplotlib.pyplot as plt
import argparse, csv, pdb
import pandas as pd 
import random
import matplotlib.cm as cm
from sklearn import preprocessing
import os
from matplotlib.ticker import FormatStrFormatter
from adjustText import adjust_text
import matplotlib.ticker as plticker

import pickle

# lfont=16
lfont=24
o_print=1;
legend_txt=["DRAM", "NVMM", "CT-1","CT-2"]
# implement an overlaod function for print
def overlaod_print(*args, **kwargs):
    if(o_print):
        return __builtins__.print(*args, **kwargs, flush=True)



    
def save_array_to_pickle(filename, data):
    # extract path from filename
    path = os.path.dirname(filename)
    # create directory if it does not exist
    if not os.path.exists(path):
        os.makedirs(path)
        
    with open(filename, 'wb') as file:
        pickle.dump(data, file)

def read_array_from_pickle(filename):
    # if filename does not exist, return empty array
    if not os.path.exists(filename):
        print(f"WARN: File {filename} does not exist")
        return []
    
    with open(filename, 'rb') as file:
        data = pickle.load(file)
    
    return data

def save_array_to_file(filename, arr):
    # extract path from filename
    path = os.path.dirname(filename)
    # create directory if it does not exist
    if not os.path.exists(path):
        os.makedirs(path)
        
    with open(filename, 'w') as file:
        for item in arr:
            file.write(str(item) + "\n")

def read_array_from_file(filename, data_type=float, quiet=0):
    my_array = []

    # if filename does not exist, return empty array
    if not os.path.exists(filename):
        if not quiet:
            print(f"WARN: File {filename} does not exist")
        return my_array
    
    with open(filename, 'r') as file:
        for line in file:
            my_array.append(data_type(line.strip()))

    return my_array

def format_axis(ax,lfont=25):
    ax.tick_params(axis=u'both', which=u'both',length=0,labelsize=lfont)
    ax.set_axisbelow(True)
    # ax.grid(which='major', axis='both', zorder=999999.0)
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['left'].set_visible(True)
    ax.get_xaxis().tick_bottom()
    ax.get_yaxis().tick_left()
    
    # xlabel and ylabel
    ax.set_xlabel(ax.get_xlabel(), fontsize=lfont)
    ax.set_ylabel(ax.get_ylabel(), fontsize=lfont)

def percentage_formatter(x, pos):
        return f'{x}%'
    
def fix_axis_percentage(ax):  
    ax.yaxis.set_major_formatter(
        matplotlib.ticker.FuncFormatter(percentage_formatter)     )

def fix_axis_int(ax):   
    ax.get_yaxis().set_major_formatter(
    matplotlib.ticker.FuncFormatter(lambda x, p: format(int(x), ','))
    )

def geo_mean(numbers):
    numbers = [number for number in numbers if (number != 0 and number != None)]
    # print("numbers: ", numbers)
    # Check if the sequence is empty
    if len(numbers) == 0:
        return None
    a = np.array(numbers)
    return a.prod()**(1.0/len(a))

def geometric_mean(numbers):
    # Remove any zeros from the sequence
    numbers = [number for number in numbers if number != 0]

    # Check if the sequence is empty
    if len(numbers) == 0:
        return None

    # Compute the geometric mean using the logarithmic form
    n = len(numbers)
    log_sum = sum(math.log(number) for number in numbers)
    gmean = math.exp(log_sum / n)

    # Return the geometric mean
    return gmean

def next_average(current_average, numbers):
    # Check for empty list
    if len(numbers) == 0:
        return current_average

    # Compute the new average
    sum_numbers = sum(numbers)
    count_numbers = len(numbers)
    new_average = (sum_numbers + current_average * count_numbers) / (count_numbers + 1)

    # Return the new average
    return new_average

def running_average(numbers):
    # print("----------running_average-------------")
    running_sum = 0
    averages = []
    for i, number in enumerate(numbers):
        running_sum += number
        averages.append(running_sum / (i + 1))
    return averages