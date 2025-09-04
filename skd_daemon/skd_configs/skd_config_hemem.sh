# ensure HEMEM_MODE is defined
if [ -z ${HEMEM_MODE} ]; then
	echo "Error: HEMEM_MODE is not defined"
	exit 1
fi

# percentile_arr=[25,50,75,90,95,99]
SKD_HOTNESS_THRESHOLD=90
SKD_MODE=$HEMEM_MODE

SKD_SLOW_TIER=-1


