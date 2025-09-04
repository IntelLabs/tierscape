all_checks_ok=1

if [ $EUID -ne 0 ]
    then echo "Must run as root or with sudo."
    exit 1
fi




# check_machine() {
#     kernel_name=$(uname -r)
#     required_kernel_name="5.17.0-ntier-noiaa-v1+"

#     if [[ $kernel_name != $required_kernel_name ]]; then
#         echo "WARNING: Wrong machine ${kernel_name}"
#         # all_checks_ok=1
#         ENABLE_NTIER=0
#     fi

#     hostname=$(cat /etc/hostname)

#     if [[ $hostname == "optaneparlab01" ]]; then
#         # echo "This is the 2-socket machine"
#         DRAM_NODES="0,1"
#         OPTANE_NODES="2,3"
#     else
#         # print error and exit
#         echo "Error: This is not the 2-socket machine"
#         all_checks_ok=0
#         exit 1
#     fi
# }


# --------------------------------


# MASIM_HOME="/data/sandeep/git_repo/parl_skd/masim_src"
# SKD_HOME="/data/sandeep/git_repo/parl_skd/sk_daemon"
# REDIS_CONF="${HOME_DIR}/redis.conf"
# DAMO_HOME="/home/ptp/alan_damo"
# YCSB_HOME="/data/sandeep/git_repo/YCSB"

# export MASIM_HOME
# export SKD_HOME
# export REDIS_CONF
# export DAMO_HOME
# export YCSB_HOME


check_path() {
    # create an array of path and ensure all of the exists
    # paths=(
    #     $MASIM_HOME
        
    #     $REDIS_CONF
    #     $DAMO_HOME
    #     $YCSB_HOME
        
    # )
    # appent /tmp to paths
    if [[ $ENABLE_NTIER -eq 1 ]]; then
        echo "ENABLE_NTIER is 1. Appending path"
        paths+=($SKD_HOME)
        paths+=("/sys/module/zswap/parameters/ntier_enabled")
        paths+=("${HOME_DIR}/shell_scripts/setup_ntiers.sh")
        paths+=("${HOME_DIR}/shell_scripts/enable_zram.sh")
        paths+=("${HOME_DIR}/convolve.py")
    fi

    for path in "${paths[@]}"; do
        if [[ ! -d $path ]]; then

            # check if this is a file
            if [[ ! -f $path ]]; then

                echo "Path $path does not exist"
                all_checks_ok=0
                break
            fi
            # exit 1
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


# check_machine
flush_trace_file
check_path
check_all_checks_ok
THREADS=$(nproc)
