# ensure FAST_NODES is defined
if [ -z ${FAST_NODES} ]; then
	echo "Error: FAST_NODES is not set"
	exit 1
fi

NTIER_HOME="exec_scripts/"
BENCH="memcached"
EXEC_MODE_BENCH="memtier_ops_900K"
MAIN_BENCH=0

export BENCH
export EXEC_MODE_BENCH
export MAIN_BENCH


# non export

# WORKLOAD_FILE=memtier_memcached_gauss_1K.cfg

WORKLOAD_FILE=memtier_memcached_gauss_4K.cfg



WORKLOAD_CONFIG="LARGE"
if [ ${WORKLOAD_CONFIG} == "PROD" ]; then
    TEST_TIME=600
    # TEST_TIME=800
    elif [ ${WORKLOAD_CONFIG} == "LARGE" ]; then
    TEST_TIME=100
    elif [ ${WORKLOAD_CONFIG} == "SMALL" ]; then
    TEST_TIME=60
fi

function exec_bench_run(){
    BENCH_RUN="memtier_benchmark $(cat ${NTIER_HOME}/config/${BENCH}/${WORKLOAD_FILE}) --out-file=${BENCH_SUMMARY_FILE} --key-pattern=G:G --ratio 0:1 --key-stddev=262144 --key-median=5242880 -t 96 -n 900000"
    export BENCH_RUN
}

export -f exec_bench_run


function exec_pre_run() {

    BENCHMARK_PID=$(pidof ${BENCH})
    if [  -z ${BENCHMARK_PID} ]; then
        echo "Error: BENCHMARK_PID is not set"
        exit 1
    fi

    echo "Pre-run script -- memtier memcached. "
    echo "Migrating pages from Optane nodes ${SLOW_NODES} to DRAM nodes ${FAST_NODES}"
    MIG_CMD="migratepages $(pidof ${BENCH}) ${SLOW_NODES} ${FAST_NODES}"
    echo "Running command: ${MIG_CMD}"
    eval ${MIG_CMD}
    

    pkill -9 memtier_benchmark

    return 0
}

function exec_post_run(){
	echo "Post-run script -- Does nothing"
}

export -f exec_pre_run
export -f exec_post_run
