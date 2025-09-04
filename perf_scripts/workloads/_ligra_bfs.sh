if [ -z ${WORKLOAD_CONFIG} ]; then
	echo "WORKLOAD_CONFIG is not set"
	exit 1
fi

if [ -z ${EXP_NAME} ]; then
	echo 
	EXP_NAME="test"
	export EXP_NAME
fi

# configuration
BENCH="BFS"
EXEC_MODE_BENCH="ligra"
WORKLOAD_FILE="rMat_100000000"
MAIN_BENCH=1
WIN_SIZE=60
SKD_DELAY=20
PUSH_THREADS=2


if [ ${WORKLOAD_CONFIG} == "PROD" ]; then
	# TEST_TIME=100
	TEST_TIME=60
elif [ ${WORKLOAD_CONFIG} == "LARGE" ]; then
	TEST_TIME=30
elif [ ${WORKLOAD_CONFIG} == "SMALL" ]; then

	TEST_TIME=15
fi

export BENCH
export EXEC_MODE_BENCH
export MAIN_BENCH
export WIN_SIZE
export SKD_DELAY
export WORKLOAD_FILE
export TEST_TIME
export PUSH_THREADS

export_get_agg_val(){
	info "No changes in threshold from default in ${BENCH}"
	return 0
}

export export_get_agg_val


export_bench_run() {
	# ensure MAIN_LOG_DIR is set
	if [ -z ${MAIN_LOG_DIR} ]; then
		echo "MAIN_LOG_DIR is not set"
		exit 1
	fi

	BENCH_RUN="/data/sandeep/git_repo/workloads/ligra-master/apps/${BENCH} -maxiters 10000 -rounds ${TEST_TIME} -s /data/sandeep/git_repo/workloads/ligra-master/utils/${WORKLOAD_FILE}"
	export BENCH_RUN

}
export export_bench_run
