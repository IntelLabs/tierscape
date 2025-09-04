# ensure root
if [ $EUID -ne 0 ]; then
    echo "Error: This script must be run as root" 2>&1 | tee -a $SCRIPT_LOG
    exit 1
fi


SKD_HOME_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "SKD_HOME_DIR: $SKD_HOME_DIR"
export SKD_HOME_DIR
EVAL_DIR="${SKD_HOME_DIR}"
export EVAL_DIR

# MUST be here==============
source "${SKD_HOME_DIR}/utils/run_utils.sh"
source "${SKD_HOME_DIR}/skd_config.sh"
source "${SKD_HOME_DIR}/utils/skd_vars.sh"
source ${PS_HOME_DIR}/profile_config.sh

ENV_FILE="/tmp/runall_env.sh"
# ===============================================

ENABLE_PROFILING=1
# if PS_HOME_DIR is not set, exit
if [ -z ${PS_HOME_DIR} ] || [ ! -d ${PS_HOME_DIR} ]; then
    echo "Error: PS_HOME_DIR is not set or does not exist. PS_HOME_DIR:'${PS_HOME_DIR}'"
    echo "Please set PS_HOME_DIR to the path of the parl_memory_perf_scripts directory"
    ENABLE_PROFILING=0
fi

ENABLE_SKD=1
export ENABLE_SKD

# DO NOT CHANGE THE ORDER -- THings will break.


# =========================================================
# # ensure BENCH_SCRIPT is defined
if [ -z ${BENCH_SCRIPT} ]; then
    if [ $# -lt 1 ]; then
        echo "Usage: $0 <BENCH_SCRIPT>"
        exit 1
    else
        BENCH_SCRIPT=$1
    fi
fi
export BENCH_SCRIPT
source ${BENCH_SCRIPT}

# ==========================


# =============== Setup SKD  -- No EXEc=================

function PREP_AND_EXECUTE_SKD(){
    
    
    # if ENABLE_SKD is 0 then return
    if [ ${ENABLE_SKD} -eq 0 ]; then
        echo "SKD: SKD is disabled. Exiting"
        return
    fi
    
    # set the skd_log files
    # starts logging to log files .. must come after SKD_SET_LOG_FILES
    
    SKD_VALIDATE
    
    SKD_PREP_SYSTEM
    
    SKD_BUILD_TRACKER_D
    
    SKD_WAIT_AND_GET_BENCH_PID
    SKD_BUILD_SKDCMD

    SKD_EXPORT_ENV_VARS
    
    SKD_EXECUTE_TRACKER_D &
    
    
}


# ========================== SETUP SKD===============

rm -f /tmp/skd_env.sh
SKD_RELATED_CLEANUP

RESET_ZSWAP_TIERS

# ==================
echo "SKD_OPS_MODE: ${SKD_OPS_MODE}"

if [ ${ENABLE_SKD} -eq 1 ]; then
    # source "${SKD_HOME_DIR}/skd_configs/skd_config_hemem.sh"
    # source ${SKD_HOME_DIR}/skd_configs/skd_config_waterfall.sh
    source ${SKD_HOME_DIR}/skd_configs/skd_config_ilp.sh
    
    # override SKD_HOTNESS_THRESHOLD here.
    # SKD_WIN_SIZE=15
    # DISABLE_MIGRATION=0
    # REMOTE_MODE=0
    # SKD_HOTNESS_THRESHOLD=.1

    # if ENV_FILE is set, then source it
    if [ -f ${ENV_FILE} ]; then
        source ${ENV_FILE}
    fi
    
fi


VALIDATE_AND_EXPORT_SKD_SETTING

PREFIX="${SKD_OPS_MODE}"
if [ ${ENABLE_SKD} -eq 1 ]; then
    PREFIX+="-F${SKD_FREQ}"

    

    if [ ${DISABLE_MIGRATION} -eq 1 ]; then
        PREFIX+="-D${DISABLE_MIGRATION}"
    else
    PREFIX+="-HT${SKD_HOTNESS_THRESHOLD}"    
    # if skd_ops_mode is ILP
    if [ ${SKD_OPS_MODE} == "ILP" ]; then
        PREFIX+="-R${REMOTE_MODE}"
    elif [ ${SKD_OPS_MODE} == "HEMEM" ]; then
        PREFIX+="-ST${SKD_SLOW_TIER}"
    fi
    PREFIX+="-PT${SKD_PUSH_THREADS}-W${SKD_WIN_SIZE}"

    fi
fi

export PREFIX
# ======================

SKD_SET_LOG_FILES
SKD_EXPORT_ENV_VARS

# exit 1

mkdir -p ${MAIN_LOG_DIR}
chmod 777 $MAIN_LOG_DIR
# skd_start_logging

# ========================== SETUP PRPFILE===============

source ${PS_HOME_DIR}/run_profile.sh
echo  "TREND_SLEEP_DURATION: ${TREND_SLEEP_DURATION}"
echo  "PERF_TIMER: ${PERF_TIMER}"


cp /tmp/skd_env.sh ${MAIN_LOG_DIR}
rm -f /tmp/skd_env.sh

# # ============================ Execute SKD ======================
PREP_AND_EXECUTE_SKD &
# # ============================ Execute Bench Script ======================

execute_bench_script

# =======SKD cleanup

SKD_RELATED_CLEANUP


chmod -R 777 ${MAIN_LOG_DIR}