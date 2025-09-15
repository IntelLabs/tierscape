all_checks_ok=1
rm -f /tmp/tierscape_sanity_check_passed


# get script dir
SANITY_SCRIPT_DIR=$(dirname $(realpath $0))
# if BASE_DIR is not set, set it to parent of SANITY_SCRIPT_DIR
if [ -z "$BASE_DIR" ]; then
    echo "BASE_DIR is not set. Setting it to parent of SANITY_SCRIPT_DIR"
    BASE_DIR=$(dirname ${SANITY_SCRIPT_DIR})/..
fi

source ${SANITY_SCRIPT_DIR}/../skd_config.sh


if [ $EUID -ne 0 ]
    then echo "Must run as root or with sudo."
    exit 1
fi


# TODO add check to ensure FAST_NODE and SLOW_NODE are set and same in model.h and skd_config.sh

check_path() {
    rm -f /tmp/skd_path_missing
    # create an array of path and ensure all of the exists
    paths=(
        ${PS_HOME_DIR}
        ${ILP_HOME_DIR}
        ${PERF_BIN}
        $MASIM_HOME        
    )
    # appent /tmp to paths
    if [[ $ENABLE_NTIER -eq 1 ]]; then
        echo "Sanity check: ENABLE_NTIER is 1. Appending path"
        paths+=($SKD_HOME)
        paths+=("/sys/module/zswap/parameters/ntier_enabled")
        paths+=("${SANITY_SCRIPT_DIR}/setup_ntiers.sh")
        paths+=("${SANITY_SCRIPT_DIR}/enable_zram.sh")
        paths+=("${SANITY_SCRIPT_DIR}/../sk_daemon/convolve.py")
    fi

    for path in "${paths[@]}"; do
        echo "Checking path: $path"
        if [[ ! -d $path ]]; then

            # check if this is a file
            if [[ ! -f $path ]]; then

                echo "Path $path does not exist"
                all_checks_ok=0
                touch /tmp/skd_path_missing
                break
            fi
            # exit 1
        fi
    done
}

# function check command /usr/bin/perf list returns 0
function check_perf() {
    ${PERF_BIN} list &>/dev/null
    if [ $? -ne 0 ]; then
        echo "Error: perf command not found"
        all_checks_ok=0
    fi
}



check_all_checks_ok() {
    if [[ $all_checks_ok -eq 0 ]]; then
        echo "Some checks failed. Exiting"
        exit 1
    else
        echo "All checks passed"
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

# if ENABLE_NTIER is set, ensure the kernel name is 5.17.0-ntier-noiaa-v1+
if [[ $ENABLE_NTIER -eq 1 ]]; then
    KERNEL_NAME=$(uname -r)
    if [[ $KERNEL_NAME != 5.17.0-ntier-noiaa-v1* ]]; then
        echo "Error: ENABLE_NTIER is set, but kernel is not 5.17.0-ntier-noiaa-v1+. Current kernel: $KERNEL_NAME" > /dev/stderr
        echo "Please install and boot into the correct kernel" > /dev/stderr
        exit 1
    else
        echo "Kernel check passed: $KERNEL_NAME" > /dev/stderr
    fi
fi


# check_machine
flush_trace_file
check_path
check_perf
check_all_checks_ok
THREADS=$(nproc)

touch /tmp/tierscape_sanity_check_passed