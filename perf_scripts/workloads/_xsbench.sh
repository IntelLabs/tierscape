if [ -z ${WORKLOAD_CONFIG} ]; then
    echo "WARN: WORKLOAD_CONFIG is not set using SMALL"
    WORKLOAD_CONFIG="SMALL"
fi


BENCH="XSBench"
EXEC_MODE_BENCH="xsbench"
WORKLOAD_FILE="XL"
MAIN_BENCH=1

# WIN_SIZE=100
# SKD_DELAY=20
# PUSH_THREADS=2

export BENCH
export EXEC_MODE_BENCH
export MAIN_BENCH


# non export

if [ ${WORKLOAD_CONFIG} == "PROD" ]; then
    TEST_TIME=5000
    elif [ ${WORKLOAD_CONFIG} == "LARGE" ]; then
    TEST_TIME=500
    WIN_SIZE=30
    elif [ ${WORKLOAD_CONFIG} == "SMALL" ]; then
    TEST_TIME=500
fi


export BENCH
export EXEC_MODE_BENCH
export MAIN_BENCH

# export WIN_SIZE
# export SKD_DELAY
# export PUSH_THREADS

export WORKLOAD_FILE
export TEST_TIME

export_get_agg_val(){
    info "No changes in threshold from default in ${BENCH}"
    return 0
}
export export_get_agg_val


# export_bench_run() {


BENCH_RUN="/data/sandeep/git_repo/workloads/XSBench-20/openmp-threading/XSBench -s ${WORKLOAD_FILE} -t 60 -l ${TEST_TIME}"

export BENCH_RUN

# }
# export export_bench_run

exec_pre_run(){
    # ensure ${BENCH} exists
    if [ ! -f /data/sandeep/git_repo/workloads/XSBench-20/openmp-threading/XSBench ]; then
        echo "Error: ${BENCH} does not exist"
        return 1
    fi
    
    return 0
    
}

exec_post_run(){
    
    echo "XSBench post run"
    
    
    return 0
}

export exec_pre_run
export exec_post_run