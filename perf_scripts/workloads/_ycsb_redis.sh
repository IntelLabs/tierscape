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
EXEC_MODE_BENCH="ycsb"

if [ -z ${WORKLOAD_FILE} ]; then
	WORKLOAD_FILE=workloadb
fi


MAIN_BENCH=0
WIN_SIZE=40
SKD_DELAY=20
PUSH_THREADS=2


if [ ${WORKLOAD_CONFIG} == "PROD" ]; then
	TEST_TIME=600
elif [ ${WORKLOAD_CONFIG} == "LARGE" ]; then
	TEST_TIME=100
elif [ ${WORKLOAD_CONFIG} == "SMALL" ]; then

	TEST_TIME=60
fi

export BENCH
export EXEC_MODE_BENCH
export WORKLOAD_FILE
export MAIN_BENCH
export WIN_SIZE
export SKD_DELAY
export PUSH_THREADS
export TEST_TIME

THREADS=64


export_get_agg_val() {
	# if AGG_MODE is not defined return
	if [ -z ${TCO_SAVE_PERCENT} ] || [ -z ${OPS_MODE} ]; then
		echo "TCO_SAVE_PERCENT or OPS_MODE is not defined"
		return
	fi

	# if [ $OPS_MODE == "HEMEM" ] || [ $OPS_MODE == "WATERFALL" ]; then
	# 	info "Reconfiguring TCO_SAVE_PERCENT for ${OPS_MODE}"
	# 	# if TCO_SAVE_PERCENT is 25
	# 	if [ $(echo "$TCO_SAVE_PERCENT == 25" | bc) -eq 1 ]; then
	# 		TCO_SAVE_PERCENT=25
	# 	elif [ $(echo "$TCO_SAVE_PERCENT == 50" | bc) -eq 1 ]; then
	# 		TCO_SAVE_PERCENT=50
	# 	elif [ $(echo "$TCO_SAVE_PERCENT == 75" | bc) -eq 1 ]; then
	# 		TCO_SAVE_PERCENT=75
	# 	else
	# 		warn "TCO_SAVE_PERCENT is set t a custom val ${TCO_SAVE_PERCENT}"
	# 		return
	# 	fi
	# fi

	# # if OPS_MODE is ILP
	# if [ $OPS_MODE == "ILP" ]; then
	# 	info "Reconfiguring TCO_SAVE_PERCENT for ${OPS_MODE}"
	# 	if [ $(echo "$TCO_SAVE_PERCENT == .1" | bc) -eq 1 ]; then
	# 		TCO_SAVE_PERCENT=.6
	# 	elif [ $(echo "$TCO_SAVE_PERCENT == .5" | bc) -eq 1 ]; then
	# 		TCO_SAVE_PERCENT=.7
	# 	elif [ $(echo "$TCO_SAVE_PERCENT == .9" | bc) -eq 1 ]; then
	# 		TCO_SAVE_PERCENT=.9
	# 	else
	# 		warn "TCO_SAVE_PERCENT is set to a custom val ${TCO_SAVE_PERCENT}"
	# 		return
	# 	fi
	# fi

	# export TCO_SAVE_PERCENT
	return

}
export export_get_agg_val

export_bench_run() {
	# ensure MAIN_LOG_DIR is set
	if [ -z ${MAIN_LOG_DIR} ]; then
		echo "MAIN_LOG_DIR is not set"
		exit 1
	fi

	BENCH_RUN="${YCSB_HOME}/bin/ycsb.sh run ${BENCH} -s -P ${HOME_DIR}/config/ycsb/${WORKLOAD_FILE} -p redis.host=127.0.0.1 -p redis.port=6379 -p threadcount=$THREADS -p hdrhistogram.percentiles=50,95,90,99,99.9 -p maxexecutiontime=${TEST_TIME}"
	export BENCH_RUN

}
export export_bench_run
