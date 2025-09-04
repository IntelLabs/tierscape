

if [ -z ${NTIER_FIXED_ILP} ]; then
    echo "Error: NTIER_FIXED_ILP is not defined"
    exit 1
fi
SKD_MODE=$NTIER_FIXED_ILP


SKD_HOTNESS_THRESHOLD=.1
# based on AGG_MODE, set SKD_HOTNESS_THRESHOLD and REMOTE_MODE
if [ ${AGG_MODE} -eq 0 ]; then
    # conservative
    SKD_HOTNESS_THRESHOLD=.9
elif [ ${AGG_MODE} -eq 1 ]; then
    # moderate
    SKD_HOTNESS_THRESHOLD=.5
elif [ ${AGG_MODE} -eq 2 ]; then
    # aggressive
    SKD_HOTNESS_THRESHOLD=.1
else
    echo "Error: AGG_MODE: ${AGG_MODE} is not recognized. Use 0, 1, or 2. Using default of ${SKD_HOTNESS_THRESHOLD}"
fi



REMOTE_MODE=0
REMOTE_ILP_SERVER_NAME="10.223.93.197"
export REMOTE_MODE