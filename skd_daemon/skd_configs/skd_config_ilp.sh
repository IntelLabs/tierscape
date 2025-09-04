
# ensure NTIER_FIXED_WATERFALL is defined
if [ -z ${NTIER_FIXED_ILP} ]; then
    echo "Error: NTIER_FIXED_ILP is not defined"
    exit 1
fi


SKD_HOTNESS_THRESHOLD=.1
SKD_MODE=$NTIER_FIXED_ILP

REMOTE_MODE=1
export REMOTE_MODE