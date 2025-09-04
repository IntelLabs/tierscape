# over echo echo, print SECONDS with a message
SECONDS=0


PENDING_TASK=0
export PENDING_TASK

# ensure PS_HOME_DIR is set
if [ -z $PS_HOME_DIR ]; then
    echo "PS_HOME_DIR is not set. Exiting"
    exit 1
fi

source ${PS_HOME_DIR}/utils_checks_scripts/sanity_checks.sh
source ${PS_HOME_DIR}/utils_checks_scripts/trace_utils.sh
source ${PS_HOME_DIR}/utils_checks_scripts/plot_utils.sh



WATCH_DOG() {
    TEST_TIME=$1
    sleep ${TEST_TIME}
    echo "WATCH DONE TIME IS UP. KILLIN THE APPLICATION ${BENCH} ${BENCHMARK_PID}"
    pkill -9 {BENCH}
    kill -9 ${BENCHMARK_PID}
    sleep 1
    echo "Checking if the process is killed or not"
    ps aux | grep ${BENCH} 
}

PROFILE_SET_LOG_FILES() {
    
    # PREFIX="${WIN_SIZE}-${SECOND_TIER}-${FREQ}-${OPS_MODE}-${TCO_SAVE_PERCENT}-PTh${PUSH_THREADS}"
    
    
    # PREFIX="${OPS_MODE}-HT${SKD_HOTNESS_THRESHOLD}-PT${PUSH_THREADS}-W${WIN_SIZE}"

    if [ -z ${PREFIX} ]; then
        echo "PREFIX is not set. Setting it to default"
        PREFIX="${OPS_MODE}-PC${PEBS_C_FREQ}-TS${TREND_SLEEP_DURATION}"
    fi
   
   # check if EVAL_DIR is set, if not set it to PS_HOME_DIR and exists
    if [ -z ${EVAL_DIR} ] || [ ! -d ${EVAL_DIR} ]; then
        echo "EVAL_DIR is not set or does not exist. Setting it to PS_HOME_DIR"
        EVAL_DIR=${PS_HOME_DIR}
    fi
    echo "EVAL_DIR is set to ${EVAL_DIR}"

    # if MAIN_LOG_DIR is not set, set it to EVAL_DIR
    if [ -z ${MAIN_LOG_DIR} ]; then
        echo "MAIN_LOG_DIR is not set. Setting it to EVAL_DIR"
        MAIN_LOG_DIR="${EVAL_DIR}/evaluation/eval_${BENCH}/${EXP_NAME}/${WORKLOAD_FILE}/perflog-"$PREFIX"-"$(date +"%Y%m%d-%H%M%S")
        export MAIN_LOG_DIR
    fi

    
    dir_list="
    PERF_FINAL_STATS=${MAIN_LOG_DIR}"/perf_final_stats"
    SCRIPT_LOG=${MAIN_LOG_DIR}"/scriptlog"
    BENCH_EXEC_FILE=${MAIN_LOG_DIR}/bench_exec
    BENCH_SUMMARY_FILE=${MAIN_LOG_DIR}/bench_summary
    PEBS_LOG=${MAIN_LOG_DIR}/pebslog
    
    TIER_STATS_FILE=${MAIN_LOG_DIR}/tierstats
    DAMO_OUTFILE=${MAIN_LOG_DIR}/damon.data
    PEBS_OUTFILE=${MAIN_LOG_DIR}/pebs.data
    PEBS_RAW_EVENTS=${MAIN_LOG_DIR}/pebs-raw-events
    "

    # echo ${PERF_FINAL_STATS}
    for line in $dir_list; do
        # echo "exporting $line: ${!line}"
        export "$line"
    done

    
 

}

CTRL_C() {
    echo "Ctrl+C detected. Exiting..."
    CLEAN_EXP
    exit 1
}


VALIDATE_PROFILE_ARGS() {
    

    validate_list="
    QUIT_FILE
    MAIN_LOG_DIR
    PS_HOME_DIR
    TREND_SLEEP_DURATION
    EXP_NAME
    BENCH
    WORKLOAD_FILE
    OPS_MODE
    TEST_TIME
    PERF_FINAL_STATS
    SCRIPT_LOG
    BENCH_EXEC_FILE
    BENCH_SUMMARY_FILE
    PEBS_LOG
    TIER_STATS_FILE
    PEBS_OUTFILE
    PEBS_RAW_EVENTS
    "

    for line in $validate_list; do
        if [ -z ${!line} ]; then
            echo "PROFILE FATAL: $line is not set: '${!line}'"
            exit 1
        fi
    done
    
    
    echo "All arguments are set"
    
}

function CHECK_BENCHMARK_PID() {
    if [ -z $BENCHMARK_PID ]; then
        echo "Fatal Error: Emptry Benchmark PID: $BENCHMARK_PID"
        return 1
    fi
    
    echo "Benchmark PID is "$BENCHMARK_PID
    return 0
}



WAIT_FOR_BENCH_TO_FINISH() {
    
    echo "Waiting for the benchmark to finish..."
    # wait $BENCHMARK_PID
    # loop while $BENCHMARK_PID is present
    while [ -e /proc/$BENCHMARK_PID ]; do
        # if QUIT_FILE exits return
        if [ -e ${QUIT_FILE} ]; then
            echo "PROFILE QUIT FILE FOUND. EXITING"
            return
        fi
        sleep 1
    done

    # wait for PENDING_TASK to become 0
    while [ $PENDING_TASK -ne 0 ]; do
        echo "Waiting for PENDING_TASK to become 0"
        sleep 1
    done

}

PRINT_END_STATS() {
    
    echo "Printing end stats.. UNUSED"
    # numastat ${BENCHMARK_PID} -c | tee -a $SCRIPT_LOG
    # DURATION=$SECONDS
    # echo -n "$SECONDS: "
    # echo "Execution Time (seconds): $DURATION"
    # echo -n "Total PEBS events captured: "
    # wc -l ${SKD_PEBS_LOG}
    
}

PRINT_ARGS() {
    
    echo "PS_HOME_DIR: $PS_HOME_DIR"
    echo "EXP_NAME: $EXP_NAME"
    echo "BENCH: $BENCH"
    echo "WORKLOAD_FILE: $WORKLOAD_FILE"
    echo "BENCH_RUN: $BENCH_RUN"
    
    echo "BENCHMARK_PID:" ${BENCHMARK_PID}
    echo "SKD_PEBS_LOG:" ${SKD_PEBS_LOG}
    
    echo "OPS_MODE: $OPS_MODE"
    echo "TEST_TIME: $TEST_TIME"
    
    
}


MARK_READY_FILE() {
    echo "MARKING READY FILE: ${READY_FILE}"
    touch ${READY_FILE}
    chmod 666 ${READY_FILE}
}

MARK_QUIT(){
    echo "MARKING QUIT QUIT_FILE: ${QUIT_FILE}"
    touch ${QUIT_FILE}
    chmod 666 ${QUIT_FILE}
}

CLEAN_EXP() {
    
    
    # if an args is passed
    if [ $# -eq 0 ]; then
        echo "No args passed. Setting MARK_QUIT"
        MARK_QUIT
    else
        echo "Args passed. Not setting MARK_QUIT"
    fi

    
    if [ ! -z ${BENCHMARK_PID} ]; then
        echo "Cleaning ${BENCH} PID: ${BENCHMARK_PID}"
        if [[ ${MAIN_BENCH} -eq 1 ]]; then
            echo "KILLING THE WORKLOAD"
            kill -9 ${BENCHMARK_PID}
            pkill ${BENCH}
            sleep 1
        else
            echo "NOT KILLING THE WORKLOAD"
        fi
    fi
        

    echo "Cleaning perf"
    for perf_pid in $(pidof perf); do
        echo "Killing Perf with PID "${perf_pid}
        kill -INT ${perf_pid}
    done
    

    
    # echo "Cleaning sar"
    # pkill sar
    
    
}

WAIT_FOR_ALLOCATION() {
    echo "Waiting for allocation to finish"
    while :; do
        if ! ps -p $BENCHMARK_PID >/dev/null 2>&1; then
            echo "Process $BENCHMARK_PID not found. Exiting while $1"
            return
        fi
        
        ss=$(grep -c "do_sigtimedwait" /proc/$BENCHMARK_PID/wchan)
        
        if [ $ss -eq 0 ]; then
            echo "Waiting for $1"
            sleep 1
        else
            return
        fi
    done
}


# Colors
export RED='\033[0;31m'
export YELLOW='\033[0;33m'
export BLUE='\033[0;34m'
export NC='\033[0m'  # No Color

_now() {
    date "+%Y-%m-%d %H:%M:%S"
}


pr_info() {
    echo -e "$(_now) ${BLUE}[INFO]${NC}  $*" >&2
}

pr_warn() {
    echo -e "${YELLOW}$(_now) [WARN]  $*${NC}" >&2
}

pr_err() {
    echo -e "${RED}$(_now) [ERROR] $*${NC}" >&2
}

export -f _now
export -f pr_info
export -f pr_warn
export -f pr_err


export -f CLEAN_EXP
export -f PRINT_END_STATS
export -f PRINT_ARGS
export -f WAIT_FOR_BENCH_TO_FINISH
export -f WAIT_FOR_ALLOCATION
export -f CTRL_C
export -f PROFILE_SET_LOG_FILES
export -f VALIDATE_PROFILE_ARGS

export -f CHECK_BENCHMARK_PID
export -f WATCH_DOG

export -f MARK_QUIT
export -f MARK_READY_FILE

# export -f PREP_SYSTEM
