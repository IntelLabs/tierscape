HEMEM_MODE=0
NTIER_FIXED_ILP=1
NTIER_FIXED_WATERFALL=2

REMOTE_ILP_SERVER_NAME="10.223.93.197"


SKD_PERF_EVENTS="-e cpu/event=0xd0,umask=0x81/ppu -e cpu/event=0xd0,umask=0x82/ppu -e cpu/event=0xd0,umask=0x11/ppu -e cpu/event=0xd0,umask=0x12/ppu"
SKD_FREQ=10000
SKD_WIN_SIZE=30
SKD_HOTNESS_THRESHOLD=2
SKD_SLOW_TIER=3
SKD_MODE=-1

SKD_PUSH_THREADS=2

DRAM_NODES="1"
OPTANE_NODES="3"



ENABLE_NTIER=0


DISABLE_MIGRATION=0

EXP_NAME="exp_skd_eurosys26_auto"

BASE_DIR="/data/sandeep/tierscape_il_gitrepo"

PS_HOME_DIR=${BASE_DIR}/"perf_scripts"
ILP_HOME_DIR=${BASE_DIR}/"ilp_server"
MASIM_HOME="${BASE_DIR}/masim_mod"

PERF_BIN="/data/sandeep/tierscape-ae/linux_5.17/tools/perf/perf"

SKD_OPS_MODE="BASELINE"


export ILP_HOME_DIR
export PS_HOME_DIR
export PERF_BIN

# --------------

