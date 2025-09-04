all_checks_ok=1


check_path() {
    # create an array of path and ensure all of the exists
    paths=(
        ${PERF_BIN}
        ${TREND_DIR}
    )
    
    for path in "${paths[@]}"; do
        if [[ ! -d $path ]]; then

            # check if this is a file
            if [[ ! -f $path ]]; then

                echo "Path $path does not exist"
                all_checks_ok=0
                break
            else
                echo "------- File $path exists"
            fi
            # exit 1
        else    
            echo "------- Directory $path exists"
        fi
    done
}



check_all_checks_ok() {
    if [[ $all_checks_ok -eq 0 ]]; then
        echo "Some checks failed. Exiting"
        exit 1
    fi
}

flush_trace_file(){
    timeout 2 cat /sys/kernel/debug/tracing/trace_pipe
}

function info() {
    echo -e "[INFO] \e[96m$1\e[0m"
}

function warn() {
    echo -e "[WARN] \e[93m$1\e[0m"
}

function err() {
    echo -e "[ERR] \e[91m$1\e[0m"
}



flush_trace_file
check_path
check_all_checks_ok
THREADS=$(nproc)
