# ensure DRAM_NODES is defined
if [ -z ${DRAM_NODES} ]; then
	echo "Error: DRAM_NODES is not set"
	exit 1
fi


# configuration
BENCH="masim"
WORKLOAD_FILE="default"
MAIN_BENCH=1
TEST_TIME="UNUSED"
WORKLOAD_FILE="default"

# TEST, SMALL, MEDIUM, LARGE
WORKLOAD_CONFIG="TEST"

MASIM_HOME="/data/sandeep/git_repo/parl_memory_masim_arv"
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


BENCH_RUN="numactl -p ${DRAM_NODES} -N ${DRAM_NODES} ${MASIM_HOME}/${BENCH} ${MASIM_HOME}/configs/${WORKLOAD_FILE}"
KILL_CMD="pkill -9 ${BENCH}"

export -f exec_pre_run
export -f exec_post_run

export BENCH
export BENCH_RUN
export KILL_CMD
export MAIN_BENCH
export WORKLOAD_FILE
export TEST_TIME