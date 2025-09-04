# ensure BNEH_SCRIPT is defined
if [ -z ${BENCH_SCRIPT} ]; then
    echo "Error: BENCH_SCRIPT is not defined"
    echo "Source this file.. DO NOT RUN IT. used profile_driver.sh to profile a workload"
    # echo "Usage: $0 <BENCH_SCRIPT> [WORKLOAD_FILE]"
    # exit 1
fi


PS_HOME_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "PS_HOME_DIR: $PS_HOME_DIR"
export PS_HOME_DIR

global_error_code=0

# ============================ READ ARGS===============================================

function check_ret_val() {
    
    if [ $global_error_code -ne 0 ]; then
        pr_err "Error: failed with Global error code $global_error_code" 
        CLEAN_EXP
        exit 1
    fi
    
    if [ $1 -ne 0 ]; then
        pr_err "Error: CALL failed with return code $1" 
        global_error_code=$1
        CLEAN_EXP
        exit 1
    fi
    
}


function run_workload() {
    
    # check if exec_pre_run function is defined
    if [ ! -z $(type -t exec_pre_run) ]; then
        echo "Running pre benchmark script" 
        exec_pre_run 
        check_ret_val $?
    fi
    
    

    if [ ! -z $(type -t exec_bench_run) ]; then
        echo "Running exec_bench_run" 
        exec_bench_run
    fi

    # ensure BENCH_RUN is defined
    if [ -z "${BENCH_RUN+x}" ]; then
        echo "Error: BENCH_RUN is not defined" 
        exit 1
    fi
    

    MARK_READY_FILE

    echo -n "$SECONDS: " 2>&1 | tee -a $SRIPT_LOG
    echo "Executing the workloads now..." 2>&1 | tee -a $SRIPT_LOG

    echo $BENCH_RUN 2>&1 | tee -a ${SCRIPT_LOG}
    touch ${BENCH_EXEC_FILE}
    chmod 777 ${BENCH_EXEC_FILE}
    $BENCH_RUN 2>&1 | tee -a ${BENCH_EXEC_FILE}
    
    if [ ! -z $(type -t exec_pre_run) ]; then
        echo "Running post benchmark script" 
        exec_post_run 
    fi

    MARK_QUIT
    
    return 0
    
}

# ============================ START UP====================================

function get_bench_pid() {
    
    echo "Getting PID" 
    
    BENCHMARK_PID=$(pidof ${BENCH})
    
    max_tries=10
    while [ -z ${BENCHMARK_PID} ]; do
        sleep .2
        BENCHMARK_PID=$(pidof ${BENCH})
        max_tries=$((max_tries-1))
        if [ $max_tries -le 0 ]; then
            echo "Error: Could not get PID of ${BENCH}" 
            exit 1
        fi
    done
    export BENCHMARK_PID
    CHECK_BENCHMARK_PID 
    
}

#======================  MONITORING SETUP===============================

function start_monitoring() {
    
    echo "Starting perf monitoring after sleeping for 1 second" 
    
    # # Sleep is required to ensure the pid is detectable... otherwise we were getting error that pid does not exist.
    while [ ! -e /proc/${BENCHMARK_PID}/exe ]; do
        sleep 0.01
    done
    
    sleep .5
    
    
    START_PERF_END_TO_END
    START_PERF_TREND
    START_PEBS_HOTNESS_LOGGING
    
}

is_setup_done=0

    
    source ${PS_HOME_DIR}/profile_config.sh
    echo "Using BENCH_SCRIPT: ${BENCH_SCRIPT}"
    # source order .. do not change.
    source ${BENCH_SCRIPT} # Need to get experiment name and other onfig
    # source ${PS_HOME_DIR}/utils_checks_scripts/default_vars.sh # Whatever is not configured by the bench_script, but is required.
    source ${PS_HOME_DIR}/utils_checks_scripts/run_utils.sh
    
    
    
    
    echo "Setting up VARS DONE ---------" 


function execute_bench_script(){
    
    # if setup is not done, do it
    if [ ${is_setup_done} -eq 0 ]; then
        # source_files
        PROFILE_SET_LOG_FILES
        is_setup_done=1
    else
        echo "Setup already done" 
    fi

    
    VALIDATE_PROFILE_ARGS

    echo "Logging to ${SCRIPT_LOG}"
    exec > >(tee -a "${SCRIPT_LOG}") 2>&1
    
         
    if [ -d ${MAIN_LOG_DIR} ]; then
        echo "Directory ${MAIN_LOG_DIR} already exists. "
    else
        mkdir -p $MAIN_LOG_DIR
        chmod 777 $MAIN_LOG_DIR
    fi

    CLEAN_EXP 1
    
    trap CTRL_C SIGINT
    # PREP_SYSTEM ${AUTONUMA_MODE} 
    
    
    PRINT_ARGS | tee -a $SCRIPT_LOG
    
    if [ ${MAIN_BENCH} -eq 1 ]; then
        echo "This is the main benchmark" 
        
        run_workload &
        sleep 1

        # wait for READY_FILE to be created
        while [ ! -e ${READY_FILE} ]; do
            sleep 0.1
        done
        echo "READY_FILE found" 
        
        get_bench_pid
        
        start_monitoring
        
    else
        # not main bench, like memcached
        echo "This is NOT the main benchmark" 
        run_workload &
        sleep 1

          # wait for READY_FILE to be created
        while [ ! -e ${READY_FILE} ]; do
            sleep 0.1
        done
        echo "READY_FILE found" 
        
        echo "Getting PID" 
        get_bench_pid
        echo "Starting monitoring" 
        start_monitoring
        echo "Running workload" 
        
    fi
    
    
    
    echo "Waiting for benchmark to finish" 
    WAIT_FOR_BENCH_TO_FINISH
    PRINT_END_STATS
    echo "****success****" 
    CLEAN_EXP
    sleep 2
    # PROFILE_PLOT_FIGS 1
}



# export functions
export -f check_ret_val
export -f get_bench_pid
export -f run_workload
export -f start_monitoring
export -f execute_bench_script

echo "REMOVING QUIT AND READY FILES: ${QUIT_FILE} ${READY_FILE}"
mkdir -m 0777 -p ${TMP_DIR}
# execute command and print output
sudo rm -f ${QUIT_FILE} ${READY_FILE}
rm -f ${QUIT_FILE} ${READY_FILE}

sleep .2
# ensure QUIT_FILE and READY_FILE are removed
if [ -e ${QUIT_FILE} ] || [ -e ${READY_FILE} ]; then
    echo "Error: Quit file ${QUIT_FILE} exists" 
    exit 1
fi


