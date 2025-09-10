# ensure FAST_NODE is defined
if [ -z ${FAST_NODE} ]; then
	echo "Error: FAST_NODE is not set"
	exit 1
fi



# configuration
BENCH="masim"
MAIN_BENCH=1
TEST_TIME="UNUSED"
WORKLOAD_FILE="stairs_plot_1gb_time"

# TEST, SMALL, MEDIUM, LARGE
WORKLOAD_CONFIG="TEST"

# if skd_env.sh exists, source it
if [ -f /tmp/skd_env.sh ]; then
	source /tmp/skd_env.sh
	MASIM_HOME=${BASE_DIR}/masim_mod
else
	MASIM_HOME="/data/sandeep/git_repo/parl_memory_masim_arv"
fi


export MASIM_HOME


exec_pre_run(){
	
	# export PRE_BENCH_RUN

	# ensure ${MASIM_HOME}/${BENCH} exists
	if [ ! -f ${MASIM_HOME}/${BENCH} ]; then
		echo "Error: ${MASIM_HOME}/${BENCH} does not exist"
		exit 1
	fi
	# ensure ${MASIM_HOME}/configs/${WORKLOAD_FILE} exists
	if [ ! -f ${MASIM_HOME}/configs/${WORKLOAD_FILE} ]; then
		echo "Error: ${MASIM_HOME}/configs/${WORKLOAD_FILE} does not exist"
		exit 1
	fi
	rm -rf /tmp/masim_addr
	return 0

}

exec_post_run(){
	rm -rf /tmp/masim_addr
}


BENCH_RUN="numactl -p ${FAST_NODE} ${MASIM_HOME}/${BENCH} ${MASIM_HOME}/configs/${WORKLOAD_FILE}"
KILL_CMD="pkill -9 ${BENCH}"

export -f exec_pre_run
export -f exec_post_run

export BENCH
export BENCH_RUN
export KILL_CMD
export MAIN_BENCH
export WORKLOAD_FILE
export TEST_TIME