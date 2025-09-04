
# if no arguments
if [ $# -eq 0 ]; then
	WORKLOAD_FILE=workloadb
else
	WORKLOAD_FILE=$1
fi

echo "WORKLOAD_FILE: ${WORKLOAD_FILE}"