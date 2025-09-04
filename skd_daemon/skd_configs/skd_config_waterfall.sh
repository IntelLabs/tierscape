
# ensure NTIER_FIXED_WATERFALL is defined
if [ -z ${NTIER_FIXED_WATERFALL} ]; then
    echo "Error: NTIER_FIXED_WATERFALL is not defined"
    exit 1
fi

SKD_MODE=$NTIER_FIXED_WATERFALL

# percentile_arr=[25,50,75,90,95,99]
SKD_HOTNESS_THRESHOLD=90

# based on AGG_MODE, set SKD_HOTNESS_THRESHOLD and REMOTE_MODE
if [ ${AGG_MODE} -eq 0 ]; then
    # conservative
    SKD_HOTNESS_THRESHOLD=25
elif [ ${AGG_MODE} -eq 1 ]; then
    # moderate
    SKD_HOTNESS_THRESHOLD=50
elif [ ${AGG_MODE} -eq 2 ]; then
    # aggressive
    SKD_HOTNESS_THRESHOLD=95
else
    echo "Error: AGG_MODE: ${AGG_MODE} is not recognized. Use 0, 1, or 2. Using default of ${SKD_HOTNESS_THRESHOLD}"
fi



