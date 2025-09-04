if [ -z ${WORKLOAD_CONFIG} ]; then
    echo "Warn: WORKLOAD_CONFIG is not set"
    WORKLOAD_CONFIG="PROD"
fi


BENCH="memcached"
EXEC_MODE_BENCH="ycsb"
YCSB_HOME="/data/sandeep/git_repo/workload_generator/YCSB"
NTIER_HOME="/data/sandeep/git_repo/ntier_exec_scripts"

if [ -z ${WORKLOAD_FILE} ]; then
    WORKLOAD_FILE=workloada_40GB_1K
fi

MAIN_BENCH=0

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
export TEST_TIME

THREADS=96



function exec_pre_run(){

    # if BENCH is running, return
    if ps aux | grep -v grep | grep -q memcached; then
        echo "INFO: ${BENCH} is already running. Not populating the data."
        return 1
    fi

    
    dir_list="
    ${YCSB_HOME}
    ${NTIER_HOME}
    "

    for line in $dir_list; do
    #  ensure YCSB_HOME and YCSB_CONFIG exists
    if [ ! -d ${line} ]; then
        echo "Error: ${line} does not exist"
        return 1
    fi
    done

    # ensure  ${NTIER_HOME}/config/ycsb/${WORKLOAD_FILE} exists
    if [ ! -f ${NTIER_HOME}/config/ycsb/${WORKLOAD_FILE} ]; then
        echo "Error: ${NTIER_HOME}/config/ycsb/${WORKLOAD_FILE} does not exist"
        return 1
    fi
    echo "Loading the workload"
    cat  ${NTIER_HOME}/config/ycsb/${WORKLOAD_FILE}

    # load the workload
    ${NTIER_HOME}/start_benchmark_server.sh ${BENCH}
    ${NTIER_HOME}/load_data_scripts/load_ycsb_memcached.sh ${NTIER_HOME}/config/ycsb/workloada_40GB_1K
    
}

function exec_post_run(){
	echo "ycsb memcached post run -- DOING NOTHING"
}


BENCH_RUN="${YCSB_HOME}/bin/ycsb.sh run ${BENCH} -s -P  ${NTIER_HOME}/config/ycsb/${WORKLOAD_FILE} -p threadcount=$THREADS -p memcached.hosts=127.0.0.1 -p hdrhistogram.percentiles=50,95,90,99,99.9 -p maxexecutiontime=${TEST_TIME} -p operationcount=400000000"

export BENCH_RUN

export -f exec_pre_run
export -f exec_post_run