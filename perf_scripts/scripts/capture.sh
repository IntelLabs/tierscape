

function cleanup(){
    echo "$0: Cleaning up"
    # kill pcm_memory

    # if a process pcm-memory is running, kill it
    # if [ ps aux | grep -v grep | grep -q pcm_memory ]; then
    if pgrep -f pcm_memory > /dev/null; then
        echo "$0: Killing pcm-memory"
        pkill -f pcm-memory
    fi
    
}

# ============== MAIN =================

echo "$0: BENCHMARK_PID is $BENCHMARK_PID"


while :; do
    
    if ! ps -p $BENCHMARK_PID > /dev/null 2>&1; then
        echo "[capture.sh] Process $BENCHMARK_PID not found. Exiting"
        cleanup
        exit 1
    fi
    
    if [ -e  ${QUIT_FILE} ]; then
        echo "Got QUIT command. FILE: ${QUIT_FILE} exists"
        cleanup
        exit 1
    fi
    
    
    cat /proc/vmstat >> $MAIN_LOG_DIR/vmstat
    cat /proc/meminfo >> $MAIN_LOG_DIR/meminfo
    cat /proc/pressure/memory >> $MAIN_LOG_DIR/psimemory
    
    cat /sys/devices/system/node/node*/meminfo >>$MAIN_LOG_DIR/meminfo
    cat /proc/$BENCHMARK_PID/status >>$MAIN_LOG_DIR/status
    
    numastat -p $BENCHMARK_PID -c|grep ^Total >> $MAIN_LOG_DIR/numastat

    if [ -f /proc/sys/kernel/zswap_print_stat ]; then
        # cat /proc/sys/kernel/zswap_print_stat >> $MAIN_LOG_DIR/zswap_print_stat
        sysctl kernel.zswap_print_stat=1 > /dev/null
    fi
    

    
    sleep $TREND_SLEEP_DURATION
done

