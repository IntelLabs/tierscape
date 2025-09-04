function GEN_RAW_EVENT(){
    perf_pid=$1

    # increment PENDING_TASK atomically
    PENDING_TASK=$((PENDING_TASK+1))
    

    # wait for perf to finish
    while ps -p $perf_pid >/dev/null 2>&1; do
        sleep 1
    done
    # echo "Perf PID $perf_pid finished" 
    pr_info "Perf PID $perf_pid finished" 
    pr_info "Generating raw events" 
    ${PERF_BIN} script -F trace: -F time,addr --reltime -i ${PEBS_OUTFILE} -f > ${PEBS_RAW_EVENTS}

    # delete PEBS_OUTFILE
    # pr_info "Deleting PEBS_OUTFILE" 
    # rm -f ${PEBS_OUTFILE}

    # compress teh raw events
    pr_info "Compressing raw events" 
    gzip -f ${PEBS_RAW_EVENTS}
    pr_info "Compressing raw events finished" 

    # decrement PENDING_TASK atomically
    PENDING_TASK=$((PENDING_TASK-1))

}

function START_PEBS_HOTNESS_LOGGING(){

    # if PEBS_HOTNESS is set to 1
    # then start the PEBS hotness logging
    if [ $PEBS_HOTNESS -eq 0 ]; then
        echo "PEBS hotness logging is disabled" 
        return
    fi

    PERF_RECORD_CMD="${PERF_BIN} record -d -c ${PEBS_C_FREQ} -e cpu/event=0xd0,umask=0x81/ppu -e cpu/event=0xd0,umask=0x82/ppu -e cpu/event=0xd0,umask=0x11/ppu -e cpu/event=0xd0,umask=0x12/ppu -p $BENCHMARK_PID  -o ${PEBS_OUTFILE}"
    
    echo "Starting PEBS hotness logging with CMD: ${PERF_RECORD_CMD}"

    ${PERF_RECORD_CMD}  &
    GEN_RAW_EVENT $!  &
}


function START_PERF_END_TO_END() {
    echo "Reading end to end perf events from file ${TREND_DIR}/perf-all-fmt" 
    PERF_EVENTS=$(cat ${TREND_DIR}/perf-all-fmt)
    
    echo "Starting END to END (dat) " 
    CMD="${PERF_BIN} stat -x, -o $PERF_FINAL_STATS -e $PERF_EVENTS -p $BENCHMARK_PID"
    # echo $CMD 
    $CMD  &
    
    
}

START_PERF_TREND() {
    
    CONT_PERF_EVENTS=$(cat ${TREND_DIR}/perf-trend-fmt)
    
    echo "Starting the monitor" 
    sar -SuB ${TREND_SLEEP_DURATION} >>$MAIN_LOG_DIR/sar_stats &
    $PERF_BIN stat -I $PERF_TIMER -e $CONT_PERF_EVENTS -p $BENCHMARK_PID &>${MAIN_LOG_DIR}/perf_trend &
    PERF_PID=$?
    
    ${TREND_DIR}/capture.sh &
    
    
}

export -f START_PERF_TREND
export -f START_PERF_END_TO_END
export -f START_PEBS_HOTNESS_LOGGING