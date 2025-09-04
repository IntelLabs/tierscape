if [ -z ${WORKLOAD_CONFIG} ]; then
	echo "WORKLOAD_CONFIG is not set"
	exit 1
fi

if [ -z ${EXP_NAME} ]; then
	echo 
	EXP_NAME="test"
	export EXP_NAME
fi

BENCH="redis"
EXEC_MODE_BENCH="memtier"
MAIN_BENCH=0

export BENCH
export EXEC_MODE_BENCH
export MAIN_BENCH


# non export

if [ -z ${WORKLOAD_FILE} ]; then
	WORKLOAD_FILE=memtier_redis_gauss_40G_1K.cfg
fi

if [ ${WORKLOAD_CONFIG} == "PROD" ]; then
	TEST_TIME=1800
elif [ ${WORKLOAD_CONFIG} == "LARGE" ]; then
	TEST_TIME=100
elif [ ${WORKLOAD_CONFIG} == "SMALL" ]; then
	TEST_TIME=60
fi

WIN_SIZE=100
SKD_DELAY=100

export_bench_run() {
	# ensure MAIN_LOG_DIR is set
	if [ -z ${MAIN_LOG_DIR} ]; then
		echo "MAIN_LOG_DIR is not set"
		exit 1
	fi

		BENCH_RUN="memtier_benchmark $(cat ${HOME_DIR}/config/ycsb/${WORKLOAD_FILE}) --out-file=${BENCH_SUMMARY_FILE} --test-time=${TEST_TIME}"
	export BENCH_RUN

}
export export_bench_run
