# ensure one arg is given
if [ $# -lt 1 ]; then
    echo "Usage: $0 <MAIN_LOG_DIR> ($#)"
    exit 1
fi
MAIN_LOG_DIR=$1

PS_HOME_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "PS_HOME_DIR: $PS_HOME_DIR"
export PS_HOME_DIR

# source ${PS_HOME_DIR}/utils_checks_scripts/default_vars.sh
source ${PS_HOME_DIR}/profile_config.sh


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

for line in $dir_list; do
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



python3 ./plotting/plot_numastat.py -i "$MAIN_LOG_DIR/numastat" --pdf $SAVE_PDF -ts ${TREND_SLEEP_DURATION}
python3 ./plotting/plot_psi.py -i "$MAIN_LOG_DIR/psimemory" --pdf $SAVE_PDF -ts ${TREND_SLEEP_DURATION}
python3 plotting/plot_pcm_bw.py -i ${MAIN_LOG_DIR}/pcm_memory --pdf $SAVE_PDF -ts ${TREND_SLEEP_DURATION}
python3 plotting/plot_perftrend_metric.py -i ${MAIN_LOG_DIR}/perf_trend -p 1 -ts 1 -m mem_load_retired.local_pmm
python3 ./plotting/plot_ops_and_avgops.py -i ${BENCH_EXEC_FILE} --pdf $SAVE_PDF
python3 plotting/plot_vmstat.py -i ${MAIN_LOG_DIR}/vmstat -m pgmigrate_success -p 1


if [ -f ${PEBS_RAW_EVENTS}.gz ]; then
    pr_info "plotting PEBS hotness"
    # decompress the file
    gunzip -f ${PEBS_RAW_EVENTS}.gz
    # plot the pebs hotness
    python3 plotting/scatter_based_pebs_raw.py -i ${PEBS_RAW_EVENTS} --pdf $SAVE_PDF
    # compress the file back
    gzip -f ${PEBS_RAW_EVENTS}
else
    pr_err "${PEBS_RAW_EVENTS}.gz not found"
fi