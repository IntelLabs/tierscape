# ensure root
if [ $EUID -ne 0 ]; then
    echo "Error: This script must be run as root" 2>&1 | tee -a $SCRIPT_LOG
    exit 1
fi



SKD_HOME_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "SKD_HOME_DIR: $SKD_HOME_DIR"
export SKD_HOME_DIR


# MUST be here==============

source /tmp/tierscape_env.sh
# call sanity_checks.sh
source $(dirname $(realpath $0))/shell_scripts/sanity_checks.sh
EVAL_DIR="${BASE_DIR}"
export EVAL_DIR

# ensure BASE_DIR is set
if [ -z ${BASE_DIR} ] || [ ! -d ${BASE_DIR} ]; then
    echo "Error: BASE_DIR is not set or does not exist. BASE_DIR:'${BASE_DIR}'"
    echo "Please set BASE_DIR to the path of the tierscape_il_gitrepo directory"
    exit 1
else
    echo "BASE_DIR: $BASE_DIR"
fi

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
# ensure two args are given
if [ $# -ne 3 ]; then
    echo "Usage: $0 <bench_script> <SKD_OPS_MODE: BASELINE, HEMEM, ILP, WATERFALL> AGG_MODE"
    exit 1
fi

BENCH_SCRIPT=$1
SKD_MODE=$2
# conservative:0 moderate:1 aggressive:2
AGG_MODE=$3


echo "BENCH_SCRIPT: $BENCH_SCRIPT"
echo "SKD_MODE: ${SKD_MODE}"
echo "AGG_MODE: ${AGG_MODE}"
echo "ENABLE_NTIER: ${ENABLE_NTIER}"

export BENCH_SCRIPT
export SKD_MODE
export AGG_MODE


source ${BENCH_SCRIPT}
VALIDATE_AND_EXPORT_SKD_SETTING

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



if [ ${ENABLE_SKD} -eq 1 ]; then
    if [ ${SKD_OPS_MODE} == "ILP" ]; then
        source "${SKD_HOME_DIR}/skd_configs/skd_config_ilp.sh"
        elif [ ${SKD_OPS_MODE} == "HEMEM" ]; then
        source "${SKD_HOME_DIR}/skd_configs/skd_config_hemem.sh"
        elif [ ${SKD_OPS_MODE} == "WATERFALL" ]; then
        source "${SKD_HOME_DIR}/skd_configs/skd_config_waterfall.sh"
    else
        echo "SKD_OPS_MODE: ${SKD_OPS_MODE} is not recognized. Exiting"
        exit 1
    fi
    
    
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
            # ensure SKD_SLOW_TIER is set
            if [ -z ${SKD_SLOW_TIER} ]; then
                echo "Error: SKD_SLOW_TIER is not set. Please set it in skd_config_hemem.sh"
                exit 1
            fi
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
PROFILE_PLOT_FIGS 1

# =======SKD cleanup

SKD_RELATED_CLEANUP


chmod -R 777 ${MAIN_LOG_DIR}