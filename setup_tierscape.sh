if [ $EUID -ne 0 ]
    then echo "Must run as root or with sudo."
    exit 1
fi
BASE_DIR=$(dirname $(realpath $0))
# /data/sandeep/tierscape_il_gitrepo

# -------------
function check_last_cmd_ret_code(){
    if [ $1 -ne 0 ]; then
        echo "Error: Last command failed with return code $1 for process $2"
        exit 1
    fi
}

# ----------
source ${BASE_DIR}/skd_daemon/skd_config.sh
check_last_cmd_ret_code $? source_config

${BASE_DIR}/skd_daemon/shell_scripts/sanity_checks.sh
check_last_cmd_ret_code $? sanity

cd /data/sandeep/tierscape_il_gitrepo/perf_scripts/scripts
bash gen_perf_all.sh
check_last_cmd_ret_code $? gen_perf_all
cd ${BASE_DIR}


# cd /data/sandeep/tierscape_il_gitrepo/ilp_server
# rm -rf build
# cmake -S. -Bbuild -DBUILD_DEPS:BOOL=ON
# cmake --build build --parallel
# echo "If this command fails, see /data/sandeep/tierscape_il_gitrepo/ilp_server/README.md"
# check_last_cmd_ret_code $? ilp_server_build


cd /data/sandeep/tierscape_il_gitrepo/skd_daemon/sk_daemon
make clean; make -j
check_last_cmd_ret_code $? make_skd


echo "SETUP DONE SUCCESSFULLY"