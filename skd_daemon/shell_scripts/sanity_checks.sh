all_checks_ok=1
rm -f /tmp/tierscape_sanity_check_passed


# get script dir
# if BASE_DIR is not set, set it to parent of BASE_DIR
if [ -z "$BASE_DIR" ]; then
    echo "BASE_DIR is not set. Setting it to parent of BASE_DIR. $(dirname ${0})"
    # exit 1
    BASE_DIR=$(dirname ${0})/../..
fi

source ${BASE_DIR}/skd_daemon/skd_config.sh


if [ $EUID -ne 0 ]
    then echo "Must run as root or with sudo."
    exit 1
fi


# Check to ensure FAST_NODE and SLOW_NODE are set and same in model.h and skd_config.sh - implemented below

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
        paths+=("${BASE_DIR}/skd_daemon/shell_scripts/setup_ntiers.sh")
        paths+=("${BASE_DIR}/skd_daemon/shell_scripts/enable_zram.sh")
        paths+=("${BASE_DIR}/skd_daemon/sk_daemon/convolve.py")
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

# function to check that FAST_NODE and SLOW_NODE are consistent between model.h and skd_config.sh
function check_node_consistency() {
    MODEL_H_PATH="${BASE_DIR}/skd_daemon/sk_daemon/utils/model.h"
    
    if [[ ! -f "$MODEL_H_PATH" ]]; then
        echo "Error: model.h not found at $MODEL_H_PATH"
        all_checks_ok=0
        return
    fi
    
    # Extract FAST_NODE and SLOW_NODE from model.h
    MODEL_FAST_NODE=$(grep "#define FAST_NODE" "$MODEL_H_PATH" | awk '{print $3}')
    MODEL_SLOW_NODE=$(grep "#define SLOW_NODE" "$MODEL_H_PATH" | awk '{print $3}')
    
    # Check if values were found
    if [[ -z "$MODEL_FAST_NODE" || -z "$MODEL_SLOW_NODE" ]]; then
        echo "Error: Could not extract FAST_NODE or SLOW_NODE from model.h"
        all_checks_ok=0
        return
    fi
    
    # Compare with values from skd_config.sh (already sourced)
    if [[ "$MODEL_FAST_NODE" != "$FAST_NODE" ]]; then
        echo "Error: FAST_NODE mismatch - model.h: $MODEL_FAST_NODE, skd_config.sh: $FAST_NODE"
        all_checks_ok=0
    fi
    
    if [[ "$MODEL_SLOW_NODE" != "$SLOW_NODE" ]]; then
        echo "Error: SLOW_NODE mismatch - model.h: $MODEL_SLOW_NODE, skd_config.sh: $SLOW_NODE"
        all_checks_ok=0
    fi
    
    if [[ "$MODEL_FAST_NODE" == "$FAST_NODE" && "$MODEL_SLOW_NODE" == "$SLOW_NODE" ]]; then
        echo "Node consistency check passed: FAST_NODE=$FAST_NODE, SLOW_NODE=$SLOW_NODE"
    fi
}

# function to check that FAST_NODE and SLOW_NODE correspond to valid NUMA nodes with >10GB free memory
function check_numa_nodes() {
    local MIN_FREE_GB=10
    local MIN_FREE_BYTES=$((MIN_FREE_GB * 1024 * 1024 * 1024))
    
    # Check if numactl command is available
    if ! command -v numactl &> /dev/null; then
        echo "Error: numactl command not found. Please install numactl package."
        all_checks_ok=0
        return
    fi
    
    # Get list of available NUMA nodes
    local available_nodes_line=$(numactl --hardware | grep "available:")
    local available_nodes=$(echo "$available_nodes_line" | sed -n 's/.*(\([0-9,-]*\)).*/\1/p')
    if [[ -z "$available_nodes" ]]; then
        echo "Error: Could not determine available NUMA nodes"
        all_checks_ok=0
        return
    fi
    
    echo "Available NUMA nodes: $available_nodes"
    
    # Convert range notation (e.g., "0-1") to individual nodes for checking
    local node_list=""
    if [[ "$available_nodes" =~ ^[0-9]+-[0-9]+$ ]]; then
        # Handle range notation like "0-1"
        local start_node=$(echo "$available_nodes" | cut -d'-' -f1)
        local end_node=$(echo "$available_nodes" | cut -d'-' -f2)
        for ((i=start_node; i<=end_node; i++)); do
            node_list="$node_list $i"
        done
    else
        # Handle comma-separated list or single nodes
        node_list=$(echo "$available_nodes" | tr ',' ' ')
    fi
    
    # Check FAST_NODE
    if ! echo "$node_list" | grep -qw "$FAST_NODE"; then
        echo "Error: FAST_NODE ($FAST_NODE) is not a valid NUMA node. Available nodes: $available_nodes"
        all_checks_ok=0
    else
        # Check free memory on FAST_NODE
        local fast_node_free=$(numactl --hardware | grep "node $FAST_NODE free:" | awk '{print $4}' | sed 's/MB//')
        if [[ -n "$fast_node_free" ]]; then
            local fast_node_free_bytes=$((fast_node_free * 1024 * 1024))
            local fast_node_free_gb=$((fast_node_free / 1024))
            if [[ $fast_node_free_bytes -lt $MIN_FREE_BYTES ]]; then
                echo "Error: FAST_NODE ($FAST_NODE) has only ${fast_node_free_gb}GB free memory, minimum required: ${MIN_FREE_GB}GB"
                all_checks_ok=0
            else
                echo "FAST_NODE ($FAST_NODE) check passed: ${fast_node_free_gb}GB free memory"
            fi
        else
            echo "Warning: Could not determine free memory for FAST_NODE ($FAST_NODE)"
        fi
    fi
    
    # Check SLOW_NODE
    if ! echo "$node_list" | grep -qw "$SLOW_NODE"; then
        echo "Error: SLOW_NODE ($SLOW_NODE) is not a valid NUMA node. Available nodes: $available_nodes"
        all_checks_ok=0
    else
        # Check free memory on SLOW_NODE
        local slow_node_free=$(numactl --hardware | grep "node $SLOW_NODE free:" | awk '{print $4}' | sed 's/MB//')
        if [[ -n "$slow_node_free" ]]; then
            local slow_node_free_bytes=$((slow_node_free * 1024 * 1024))
            local slow_node_free_gb=$((slow_node_free / 1024))
            if [[ $slow_node_free_bytes -lt $MIN_FREE_BYTES ]]; then
                echo "Error: SLOW_NODE ($SLOW_NODE) has only ${slow_node_free_gb}GB free memory, minimum required: ${MIN_FREE_GB}GB"
                all_checks_ok=0
            else
                echo "SLOW_NODE ($SLOW_NODE) check passed: ${slow_node_free_gb}GB free memory"
            fi
        else
            echo "Warning: Could not determine free memory for SLOW_NODE ($SLOW_NODE)"
        fi
    fi
    
    # Check that FAST_NODE and SLOW_NODE are different
    if [[ "$FAST_NODE" == "$SLOW_NODE" ]]; then
        echo "Error: FAST_NODE and SLOW_NODE cannot be the same ($FAST_NODE)"
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
check_node_consistency
check_numa_nodes
check_all_checks_ok
THREADS=$(nproc)

touch /tmp/tierscape_sanity_check_passed