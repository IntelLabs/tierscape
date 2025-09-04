import numpy as np
import sys
from scipy.stats import gmean

isdebug=0
# read output_file as first argument
if len(sys.argv) > 1:
    output_file = sys.argv[1]
else:
    output_file = "/tmp/gauss"

comm_file = "/tmp/comm"

if isdebug==1:
    arr = np.array([1,0,30,0,0,0,300,0,0,7,0,0,0,0,15])
else:
    # read arr from /tmp/hotness
    arr = []
    with open("/tmp/hotness", "r") as f:
        for line in f:
            arr.append(int(line.strip()))
        
  

weights = np.array([.1,.2,.5,.2,.1])
weights = np.array([.1,.3,.4,.8,.4,.3,.1])
# weights = np.array([.1,.2,.3,.4,.5,.6,.7,.8,.9,.8,.7,.6,.5,.4,.3,.2,.1])
updated_arr = np.convolve(arr, weights, mode='same')
updated_arr = [int(b) if int(b) > a else a for a, b in zip(arr, updated_arr)]


if isdebug==1:
    print(arr)
    print(updated_arr)
else:
    # save updated_arr to /tmp/hotness_updated
    with open("/tmp/hotness_updated", "w") as f:
        for i in updated_arr:
            f.write(str(i) + "\n")

# =========================================================


# fill gaussian distribution to updated_arr and print mean, std, and other relevant details
percentile_arr=[25,50,75,90,95,99]
def print_stats(input_arr):
    mean = np.mean(input_arr)
    std = np.std(input_arr)
    g_val = gmean(input_arr)
    percentiles = (np.percentile(input_arr,percentile_arr ))
    percentiles = [round(i,2) for i in percentiles]
    print(f"mean: {round(mean,2)} gmean: {round(g_val,2)} std: {round(std,2)}")
    print(f"percentiles: {percentiles}")
    return mean,std,g_val,percentiles


non_zero_updated_arr = [i for i in updated_arr if i > 0]
print("\n--- Convolve stats----")
print(f"len updated_arr: {len(updated_arr)} len non_zero_updated_arr: {len(non_zero_updated_arr)}")

print("With zero entries: ", end=" ")
mean,std,g_val,percentiles = print_stats(updated_arr)

print("Without zero entries: ", end=" ")
mean,std,g_val,percentiles = print_stats(non_zero_updated_arr)

with open(comm_file, "w") as f:
    for i in range(len(percentile_arr)):
        f.write(str(percentiles[i]) + " ")
    f.write("\n")

with open(output_file, "a") as f:
    f.write(f"With non zero: mean {round(mean,2)} gmean {round(g_val,2)} std {round(std,2)} ")
    for i in range(len(percentile_arr)):
        f.write(str(percentiles[i]) + " ")
    f.write("\n")

print("--- Convolve stats END----")
# ========================


    

# with open(output_file, "a") as f:
#     f.write(str(round(mean,2)) + " " + str(round(std,2)) + " " );
#     for i in range(len(percentile_arr)):
#         f.write(str(percentiles[i]) + " ")
#     f.write("\n")
#     # + str(percentiles[0]) + " " + str(percentiles[1]) + " " + str(percentiles[2]) + "\n")

# ======== THIS IS USED BY SKD================
