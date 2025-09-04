
# ensure NTIER_FIXED_WATERFALL is defined
if [ -z ${NTIER_FIXED_WATERFALL} ]; then
    echo "Error: NTIER_FIXED_WATERFALL is not defined"
    exit 1
fi

# percentile_arr=[25,50,75,90,95,99]
SKD_HOTNESS_THRESHOLD=90
SKD_MODE=$NTIER_FIXED_WATERFALL
