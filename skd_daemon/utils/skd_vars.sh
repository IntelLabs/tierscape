


function SKD_EXPORT_ENV_VARS(){
    # echo all the vars to /tmp/skd_env.sh
    
    echo "SKD_HOME_DIR=${SKD_HOME_DIR}" > /tmp/skd_env.sh
    echo "EVAL_DIR=${EVAL_DIR}" >> /tmp/skd_env.sh
    echo "SKD_MODE=${SKD_MODE}" >> /tmp/skd_env.sh
    echo "SKD_HOTNESS_THRESHOLD=${SKD_HOTNESS_THRESHOLD}" >> /tmp/skd_env.sh
    echo "SKD_CMD=${SKD_CMD}" >> /tmp/skd_env.sh
    echo "SKD_FREQ=${SKD_FREQ}" >> /tmp/skd_env.sh
    echo "SKD_WIN_SIZE=${SKD_WIN_SIZE}" >> /tmp/skd_env.sh
    echo "SKD_PUSH_THREADS=${SKD_PUSH_THREADS}" >> /tmp/skd_env.sh
    echo "SKD_SLOW_TIER=${SKD_SLOW_TIER}" >> /tmp/skd_env.sh
    echo "DISABLE_MIGRATION=${DISABLE_MIGRATION}" >> /tmp/skd_env.sh
    echo "ENABLE_SKD=${ENABLE_SKD}" >> /tmp/skd_env.sh
    echo "BENCH=${BENCH}" >> /tmp/skd_env.sh
    echo "PREFIX=${PREFIX}" >> /tmp/skd_env.sh
    echo "MAIN_LOG_DIR=$MAIN_LOG_DIR" >> /tmp/skd_env.sh
    echo "BASE_DIR=${BASE_DIR}" >> /tmp/skd_env.sh
    
}


function SKD_SET_LOG_FILES(){
    
    
    MAIN_LOG_DIR="${EVAL_DIR}/evaluation/eval_${BENCH}/${EXP_NAME}/${EXEC_MODE_BENCH}/${WORKLOAD_FILE}/perflog-"$PREFIX"-"$(date +"%Y%m%d-%H%M%S")
    echo "MAIN_LOG_DIR: $MAIN_LOG_DIR"
    export MAIN_LOG_DIR
    
    dir_list="
    SKDSTATFILE=${MAIN_LOG_DIR}/skd_stat
    SKD_PEBS_LOG=${MAIN_LOG_DIR}/skd_pebs_log
    SKD_LOG=${MAIN_LOG_DIR}/skd_log
    SKD_REGIONS=${MAIN_LOG_DIR}/skd_regions
    ILP_PERF_STATS=${MAIN_LOG_DIR}"/ilp_perf_stats"
    ILP_LOG=${MAIN_LOG_DIR}"/ilp_log"
    TIER_STATS_FILE=${MAIN_LOG_DIR}"/tierstats"
    "

    # echo ${PERF_FINAL_STATS}
    for line in $dir_list; do
        # echo "exporting $line: ${!line}"
        export "$line"
    done


    
}




function SKD_BUILD_TRACKER_D(){
    
    cd ${SKD_HOME_DIR}/sk_daemon
    make -j
    # ensure return 0
    if [ $? -ne 0 ]; then
        echo "SKD: make failed"
        exit 1
    fi
    cd ..
}


function SKD_VALIDATE(){
    defined_vars_list="
    BENCH
    QUIT_FILE
    SKD_HOME_DIR
    SKD_FREQ
    SKD_WIN_SIZE
    SKD_MODE
    "
    for var in $defined_vars_list; do
        if [ -z "${!var}" ]; then
            echo "SKD: $var is not set"
            exit 1
        fi
    done
}


function SKD_WAIT_AND_GET_BENCH_PID(){
    
    
    # # if an arg read BENCHMARK_PID from that, or frm the env
    # if BENCHMARK_PID is not defined
    
    if [ -z "${BENCHMARK_PID}" ]; then
        BENCHMARK_PID=$(pidof ${BENCH})
        
        max_tries=100
        echo "SKD: Waiting for PID of ${BENCH} to be set for ${max_tries} tries of .2 second each"
        while [ -z ${BENCHMARK_PID} ]; do
            sleep .2

            if [ -e ${QUIT_FILE} ]; then
                echo "SKD: QUIT_FILE exists. Exiting"
                exit 1
            fi

            BENCHMARK_PID=$(pidof ${BENCH})
            max_tries=$((max_tries-1))
            if [ $max_tries -le 0 ]; then
                echo "Error: Could not get PID of ${BENCH}"
                exit 1
            fi
        done
        
    fi
 
    
    echo "SKD: BENCHMARK_PID: $BENCHMARK_PID"
    export BENCHMARK_PID
    
}



function SKD_BUILD_SKDCMD()
{
    # a small winsize wont have time to move things
    
    
    echo "SKD_DAEMON: $BENCHMARK_PID Freq:$SKD_FREQ Wndo Size: $SKD_WIN_SIZE"
    
    
    FLAGS_CMD=""
    FLAGS_CMD+=" ${SKD_WIN_SIZE:+-w ${SKD_WIN_SIZE}}"
    FLAGS_CMD+=" ${SKD_MODE:+-o ${SKD_MODE}}"
    FLAGS_CMD+=" ${SKD_PEBS_LOG:+-l ${SKD_PEBS_LOG}}"
    FLAGS_CMD+=" ${TIER_STATS_FILE:+-s ${TIER_STATS_FILE}}"
    FLAGS_CMD+=" ${SKD_HOTNESS_THRESHOLD:+-c ${SKD_HOTNESS_THRESHOLD}}"
    FLAGS_CMD+=" ${SKD_PUSH_THREADS:+-e ${SKD_PUSH_THREADS}}"
    FLAGS_CMD+=" ${DISABLE_MIGRATION:+-d ${DISABLE_MIGRATION}}"
    FLAGS_CMD+=" ${SKD_SLOW_TIER:+-h ${SKD_SLOW_TIER}}"
    
    
    SKD_CMD="${PERF_BIN} stat -e task-clock,context-switches,cycles,instructions -x, -o ${SKDSTATFILE} ${SKD_HOME_DIR}/sk_daemon/tracker_d.o -p ${BENCHMARK_PID}"

    SKD_CMD+=${FLAGS_CMD}
    
    echo "SKD_CMD: $SKD_CMD"
    
    
    
    
}


# SKD_HOTNESS_THRESHOLD is very interestig
# for ILP, it should be between 0 to 1 as per the paper
# for HeMeM and other count based threshold, if its 25 50 75 90 95 or 99, then its percentile-based or histogram based
# else, it is a direct hotness thredhold.


function SKD_EXECUTE_TRACKER_D(){
    # ================================
    if ! ps -p $BENCHMARK_PID >/dev/null 2>&1; then
        echo "Process $PID not found. Exiting"
        exit 1
    fi
    
    echo ""
    echo "==================="
    echo "${PERF_BIN} record -d ${SKD_PERF_EVENTS} -c ${SKD_FREQ} -p ${BENCHMARK_PID} -o - | ${PERF_BIN} script -F trace: -F time,addr --reltime -i - | ${SKD_CMD}"
    echo "==================="

    ${PERF_BIN} record -d ${SKD_PERF_EVENTS} -c ${SKD_FREQ} -p ${BENCHMARK_PID} -o - | ${PERF_BIN} script -F trace: -F time,addr --reltime -i - | ${SKD_CMD} 2>&1 | tee -a ${SKD_LOG}

    
}


SKD_PREP_SYSTEM() {
    echo "Prepping the System"
    
    # if no argument is passed, set NUMA_MODE to 0
    if [ $# -eq 0 ]; then
        echo "No NUMA_MODE argument passed. Setting to 0"
        NUMA_MODE=0
    else
        echo "Setting NUMA MODE to $1"
        NUMA_MODE=$1
    fi
    
    echo "Setting the numa_balancing to $1"
    echo ${NUMA_MODE} >/proc/sys/kernel/numa_balancing
    echo false >/sys/kernel/mm/numa/demotion_enabled
    
    if [ ${NUMA_MODE} -eq 2 ]; then
        echo true >/sys/kernel/mm/numa/demotion_enabled
    fi
    
    if [ ${NUMA_MODE} -eq 2 ]; then
        echo true >/sys/kernel/mm/numa/demotion_enabled
    fi
    
    sync
    echo 3 >/proc/sys/vm/drop_caches
    
    echo never | sudo tee >/sys/kernel/mm/transparent_hugepage/enabled
    echo never | sudo tee >/sys/kernel/mm/transparent_hugepage/defrag
    
    
    echo "Disabling address space randomization"
    sudo sysctl kernel.randomize_va_space=0
    
    # log everyting to script_log
    echo -n "numa_balancing: " 
    cat /proc/sys/kernel/numa_balancing 
    
    echo -n "numa_demotion_enabled: " 
    cat /sys/kernel/mm/numa/demotion_enabled 
    
    echo -n "transparent_hugepage: " 
    cat /sys/kernel/mm/transparent_hugepage/enabled 
    
    echo -n "transparent_hugepage_defrag: " 
    cat /sys/kernel/mm/transparent_hugepage/defrag 
    
    echo -n "randomize_va_space: " 
    cat /proc/sys/kernel/randomize_va_space 
    
    echo -n "cpu_governor: " 
    cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 
    
    echo "Enabling tracing"
    echo 1 > /sys/kernel/debug/tracing/tracing_on
    
    echo "Enabling tracing"
    echo 1 > /sys/kernel/debug/tracing/tracing_on
    
    echo "OMP_NUM_THREADS: $OMP_NUM_THREADS"
}

function RESET_ZSWAP_TIERS() {

    # if not ENABLE_NTIER return
    if [[ $ENABLE_NTIER -eq 0 ]]; then
        echo "ENABLE_NTIER is 0. Not resetting zswap tiers"
        return
    else
        echo "ENABLE_NTIER is 1. Resetting zswap tiers"
    fi

    echo "Resetting zswap tiers"
    swapoff -a
    bash ${SKD_HOME_DIR}/shell_scripts/setup_ntiers.sh

    echo "Disabling prefetching"
    echo 0 >/proc/sys/vm/page-cluster
    echo 0 >/sys/kernel/mm/swap/vma_ra_enabled

}


function SKD_RELATED_CLEANUP(){

    # # if ENABLE_SKD is 0 then return
    # if [ ${ENABLE_SKD} -eq 0 ]; then
    #     echo "SKD: SKD is disabled. Exiting"
    #     return
    # fi
    
    echo "Executing SKD_RELATED_CLEANUP"

    echo "Cleaning tracker"
    for tracker_d in $(pidof tracker_d.o); do
        echo "Killing Tracker with PID "${tracker_d}
        sudo kill -2 ${tracker_d}
    done

    sleep 1
    echo "Cleaning tracker with kill "
    for tracker_d in $(pidof tracker_d.o); do
        echo "Killing tracker_d with PID "${tracker_d}
        sudo kill -9 ${tracker_d}
    done
    sleep .2
    pkill -2 tracker_d.o
    sleep .2

    # while [ $(pidof tracker_d.o) ]; do
    #     echo "Killing Tracker with PID "${tracker_pid}
    #     sudo kill -9 ${tracker_pid}
    # done
    
    echo "Cleaning skdilpserver "
    for skd_ilp_server in $(pidof skd_ilp_server); do
        echo "Killing skd_ilp_server with PID "${skd_ilp_server}
        sudo kill -9 ${skd_ilp_server}
    done
    
    
    
     USER="sandeep"
     chown $USER:$USER -R *
     chown $USER:$USER -R .*
}

export -f SKD_PREP_SYSTEM
export -f RESET_ZSWAP_TIERS