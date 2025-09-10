if [ $EUID -ne 0 ]
then echo "Must run as root or with sudo."
    exit 1
fi

# if there is an argument, it is ENABLE_NTIER
if [ $# -eq 1 ]
then
    ENABLE_NTIER=$1
else
    ENABLE_NTIER=0
fi
export ENABLE_NTIER
echo "Setup Tierscape ENABLE_NTIER: $ENABLE_NTIER"

BASE_DIR=$(dirname $(realpath $0))
echo "BASE_DIR: $BASE_DIR"
# ${BASE_DIR}

# -------------
function check_last_cmd_ret_code(){
    if [ $1 -ne 0 ]; then
        echo "Error: Last command failed with return code $1 for process $2"
        exit 1
    fi
}


# ----------
function source_skd_config(){
    source ${BASE_DIR}/skd_daemon/skd_config.sh
    
    check_last_cmd_ret_code $? source_config
}

function check_sanity(){
    bash ${BASE_DIR}/skd_daemon/shell_scripts/sanity_checks.sh 2>&1 | tee ${BASE_DIR}/logs/sanity_checks.log
    # if /tmp/skd_path_missing exists, then exit 1
    if [ -f /tmp/skd_path_missing ]; then
        echo "FATAL: One or more paths are missing. Please check the logs/sanity_checks.log file" >&2
        exit 1
    fi
    check_last_cmd_ret_code $? sanity
}

function configure_perf_scripts(){
    cd ${BASE_DIR}/perf_scripts/scripts
    bash gen_perf_all.sh 2>&1 | tee ${BASE_DIR}/logs/gen_perf_all.log
    check_last_cmd_ret_code $? gen_perf_all
    cd ${BASE_DIR}
}


function configure_ilp(){
    cd ${BASE_DIR}/ilp_server
    rm -rf build
    cmake -S. -Bbuild -DBUILD_DEPS:BOOL=ON 2>&1 | tee ${BASE_DIR}/logs/ilp_server_cmake.log
    cmake --build build --parallel 2>&1 | tee ${BASE_DIR}/logs/ilp_server_build.log
    # if last ret code is not 0
    if [ $? -ne 0 ]; then
        echo "Error: ilp_server build failed" >&2
        echo "Trying to install OR-TOOLS" >&2
        ${BASE_DIR}/ilp_server/get_latest_cmake_gcc_or_tools.sh 2>&1 | tee ${BASE_DIR}/logs/ilp_tools_install.log
        check_last_cmd_ret_code $? orl_tools_install
        echo "OR-TOOLS installation done. Trying to build ilp_server again" >&2
        cmake -S. -Bbuild -DBUILD_DEPS:BOOL=ON
        cmake --build build --parallel
        check_last_cmd_ret_code $? ilp_server_build
    fi
    check_last_cmd_ret_code $? ilp_server_build
}

function configure_skd_daemon(){
    cd ${BASE_DIR}/skd_daemon/sk_daemon
    make clean; make -j ENABLE_NTIER=${ENABLE_NTIER} 2>&1 | tee ${BASE_DIR}/logs/make_skd.log
    check_last_cmd_ret_code $? make_skd
}


function export_tierscape_env(){
    echo "BASE_DIR=$BASE_DIR" > /tmp/tierscape_env.sh
    echo "PERF_BIN=${PERF_BIN}" >> /tmp/tierscape_env.sh
    echo "PS_HOME_DIR=${PS_HOME_DIR}" >> /tmp/tierscape_env.sh
    echo "ILP_HOME_DIR=${ILP_HOME_DIR}" >> /tmp/tierscape_env.sh
    echo "ENABLE_NTIER=${ENABLE_NTIER}" >> /tmp/tierscape_env.sh
    
    # cat to stderr
    cat /tmp/tierscape_env.sh >&2
}

function update_header(){
    HEADER_FILE=${BASE_DIR}/skd_daemon/sk_daemon/utils/model.h
    # Use sed to update the header file in-place
    sed -i "s/^#define FAST_NODE .*$/#define FAST_NODE $FAST_NODE/" $HEADER_FILE
    sed -i "s/^#define SLOW_NODE .*$/#define SLOW_NODE $SLOW_NODE/" $HEADER_FILE
    
    echo "Updated $HEADER_FILE with FAST_NODE=$FAST_NODE and SLOW_NODE=$SLOW_NODE"
    
}

source_skd_config
update_header

check_sanity

# check for the presence of /tmp/tierscape_sanity_check_passed
if [ ! -f /tmp/tierscape_sanity_check_passed ]; then
    echo "FATAL: Sanity checks did not pass. Please check the logs/sanity_checks.log file" >&2
    cat logs/sanity_checks.log >&2
    exit 1
fi

configure_perf_scripts
configure_ilp

configure_skd_daemon
export_tierscape_env



echo "SETUP DONE SUCCESSFULLY" >&2