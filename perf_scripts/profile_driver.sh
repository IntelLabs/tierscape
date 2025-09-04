PS_HOME_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "PS_HOME_DIR: $PS_HOME_DIR"

export PS_HOME_DIR
PCM_HOME_DIR="/data/sandeep/git_repo/pcm"
export PCM_HOME_DIR

# DO NOT CHANGE THE ORDER -- THings will break.


# =========================================================
# # ensure BENCH_SCRIPT is defined
if [ -z ${BENCH_SCRIPT} ]; then
    # read from first arg
    if [ $# -lt 1 ]; then
        echo "Error: BENCH_SCRIPT is not defined"
        echo "Usage: $0 <BENCH_SCRIPT> [WORKLOAD_FILE]"
        exit 1
    else
        BENCH_SCRIPT=$1
    fi
fi
export BENCH_SCRIPT


# ========================== SETUP PRPFOLE===============

source run_profile.sh
source_files
echo  "TREND_SLEEP_DURATION: ${TREND_SLEEP_DURATION}"
echo  "PERF_TIMER: ${PERF_TIMER}"

PROFILE_SET_LOG_FILES

# # ============================ Execute Bench Script ======================
rm -rf ${QUIT_FILE} ${READY_FILE}


execute_bench_script

