
# ensure one arg is given
if [ $# -lt 1 ]; then
    echo "Usage: $0 <MAIN_LOG_DIR> ($#)"
    exit 1
fi
MAIN_LOG_DIR=$1


SKD_HOME_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "SKD_HOME_DIR: $SKD_HOME_DIR"
PLOT_DIR=${SKD_HOME_DIR}/plotting
export SKD_HOME_DIR
export PLOT_DIR

echo "source ${SKD_HOME_DIR}/../.tierscape_venv/bin/activate"

# ensure that dir ${SKD_HOME_DIR}/../.tierscape_venv/bin exits
if [ ! -d ${SKD_HOME_DIR}/../.tierscape_venv/bin ]; then
    echo "Error: ${SKD_HOME_DIR}/../.tierscape_venv/bin does not exist"
    echo "Run make python_setup"
    exit 1
fi

source ${SKD_HOME_DIR}/../.tierscape_venv/bin/activate

source ${MAIN_LOG_DIR}/skd_env.sh
source ${SKD_HOME_DIR}/skd_config.sh
source ${PS_HOME_DIR}/profile_config.sh

# ensure MAIN_LOG_DIR SKD_HOME_DIR and PS_HOME_DIR are set
if [ -z ${MAIN_LOG_DIR} ] || [ ! -d ${MAIN_LOG_DIR} ]; then
    echo "Error: MAIN_LOG_DIR is not set or does not exist. MAIN_LOG_DIR:'${MAIN_LOG_DIR}'"
    echo "Please set MAIN_LOG_DIR to the path of the parl_memory_perf_scripts directory"
    exit 1
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

SKDSTATFILE=${MAIN_LOG_DIR}/skd_stat
SKD_PEBS_LOG=${MAIN_LOG_DIR}/skd_pebs_log
SKD_LOG=${MAIN_LOG_DIR}/skd_log
SKD_REGIONS=${MAIN_LOG_DIR}/skd_regions

ILP_PERF_STATS=${MAIN_LOG_DIR}"/ilp_perf_stats"

"

    # echo ${PERF_FINAL_STATS}
    for line in $dir_list; do
        # echo "exporting $line: ${!line}"
        export "$line"
    done


# 1 for PNG 2 for PDF and 0 for None
# read from arg 2 else, make it 2
if [ $# -eq 2 ]; then
    SAVE_PDF=$2
else
    SAVE_PDF=1
fi

# =================================
# rm -rf ${MAIN_LOG_DIR}/plots
mkdir -p ${MAIN_LOG_DIR}/plots
rm -rf ${MAIN_LOG_DIR}/plots/info.txt
# 
# =================================


# python3 ${PLOT_DIR}/plot_numastat.py -i "$MAIN_LOG_DIR/numastat" --pdf $SAVE_PDF -ts ${TREND_SLEEP_DURATION}
python3 ${PLOT_DIR}/plot_tiers_stats.py -i ${TIER_STATS_FILE} --pdf $SAVE_PDF
# python3 ${PLOT_DIR}/plot_stacked_tco.py -d $MAIN_LOG_DIR"/plots" --pdf $SAVE_PDF -ts ${TREND_SLEEP_DURATION}

# python3 ${PLOT_DIR}/plot_skdlog.py -i ${SKD_LOG} --pdf $SAVE_PDF
# python3 ${PLOT_DIR}/plot_psi.py -i "$MAIN_LOG_DIR/psimemory" --pdf $SAVE_PDF -ts ${TREND_SLEEP_DURATION}
# python3 ${PLOT_DIR}/plot_pcm_bw.py -i ${MAIN_LOG_DIR}/pcm_memory --pdf $SAVE_PDF -ts ${TREND_SLEEP_DURATION}
# python3 ${PLOT_DIR}/plot_perftrend_metric.py -i ${MAIN_LOG_DIR}/perf_trend -p 1 -ts 1 -m mem_load_retired.local_pmm
# python3 ${PLOT_DIR}/plot_vmstat.py -i ${MAIN_LOG_DIR}/vmstat -m pgmigrate_success -p 1
# python3 ${PLOT_DIR}/plot_status.py -i ${MAIN_LOG_DIR}/status -m VmRSS -p 1

# python3 ${PLOT_DIR}/plot_ops_and_avgops.py -i ${BENCH_EXEC_FILE} --pdf $SAVE_PDF

# python3 ${PLOT_DIR}/plot_stacked_regions.py -i ${SKD_REGIONS} --pdf $SAVE_PDF


# if [ -f ${PEBS_RAW_EVENTS}.gz ]; then
#     # decompress the file
#     gunzip -f ${PEBS_RAW_EVENTS}.gz
#     # plot the pebs hotness
#     python3 ${PLOT_DIR}/scatter_based_pebs_raw.py -i ${PEBS_RAW_EVENTS} --pdf $SAVE_PDF
#     # compress the file back
#     gzip -f ${PEBS_RAW_EVENTS}
# else
#     echo "${PEBS_RAW_EVENTS}.gz not found"
# fi

deactivate
# # ${PLOT_DIR}/post_process.sh $PERF_FINAL_STATS $ILP_PERF_STATS 2>&1 | tee ${MAIN_LOG_DIR}/plots/post_process.log

