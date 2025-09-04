import pandas as pd
import numpy as np
import sys
import os
import re
import time

def read_raw_in_df(input_file):

	df = pd.read_csv(input_file, sep=":", header=None, names=["time", "addr"], on_bad_lines="warn")

	df['time'] = df['time'].astype(float)
	df['addr'] = df['addr'].str.strip().str.replace(" ", "").str.replace(":", "").str.replace("0x", "", regex=False)

	# drop where addr is 0
	df = df[df['addr'] != '0']

	print(df.head())
	return df

def process_raw_in_df(df):
	df['addr'] = df['addr'].apply(lambda x: int(x, 16) & ~0xFFF)
	# df['addr'] = df['addr'].apply(lambda x: int(x, 16))

	heap_min = "0x7ff000000000"
	heap_min_int = int(heap_min, 16)
	df = df.drop(df[df.addr < heap_min_int].index)

	df['addr'] = df['addr'].apply(lambda x: hex(x))

	df['count'] = 1
	df['time'] = df['time'].apply(lambda x: int(x // 1))
	df = df.groupby(['time', 'addr']).sum().reset_index()
	# df['addr'] = df['addr'].apply(lambda x: hex(int(x, 16) << 12))
	# df['addr'] = df['addr'].apply(lambda x: x << 12)
	# print head
	
	print("Processed data -------------")
	print(df.head())
	print(df.tail())
	print(------------------)
	# save df as csv
	output_file = os.path.splitext(input_file)[0] + "_processed.csv"
	df.to_csv(output_file, index=False)

 
input_file = sys.argv[1]
df = read_raw_in_df(input_file)
df = process_raw_in_df(df)



