if [ -z ${WORKLOAD_CONFIG} ]; then
    echo "WARN: WORKLOAD_CONFIG is not set"
    WORKLOAD_CONFIG="SMALL"
fi


BENCH="PageRank"
EXEC_MODE_BENCH="ligra"
WORKLOAD_FILE="rMat_100000000"
MAIN_BENCH=1



if [ ${WORKLOAD_CONFIG} == "PROD" ]; then
    TEST_TIME=4
    elif [ ${WORKLOAD_CONFIG} == "LARGE" ]; then
    TEST_TIME=3
    elif [ ${WORKLOAD_CONFIG} == "SMALL" ]; then
    TEST_TIME=2
fi

export BENCH
export EXEC_MODE_BENCH
export WORKLOAD_FILE
export MAIN_BENCH
export TEST_TIME

LIGRA_HOME="/data/sandeep/git_repo/workloads/ligra-master"

function exec_pre_run(){
    
    dir_list="
    ${LIGRA_HOME}
    "

    for line in $dir_list; do
    #  ensure YCSB_HOME and YCSB_CONFIG exists
    if [ ! -d ${line} ]; then
        echo "Error: ${line} does not exist"
        return 1
    fi
    done
    
}

function exec_post_run(){
	echo "ligra pagerank post run -- DOING NOTHING"
}


BENCH_RUN="${LIGRA_HOME}/apps/${BENCH} -maxiters 10 -rounds ${TEST_TIME} -s ${LIGRA_HOME}/utils/${WORKLOAD_FILE}"

export BENCH_RUN
export -f exec_pre_run
export -f exec_post_run



