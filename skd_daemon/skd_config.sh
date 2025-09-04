BASELINE_MODE=-1
HEMEM_MODE=0
NTIER_FIXED_ILP=1
NTIER_FIXED_WATERFALL=2



EXP_NAME="exp_skd_eurosys26_auto"
PERF_BIN="/data/sandeep/idxd/tools/perf/perf"

SKD_PERF_EVENTS="-e cpu/event=0xd0,umask=0x81/ppu -e cpu/event=0xd0,umask=0x82/ppu -e cpu/event=0xd0,umask=0x11/ppu -e cpu/event=0xd0,umask=0x12/ppu"
SKD_FREQ=10000
SKD_WIN_SIZE=5
SKD_HOTNESS_THRESHOLD=2

SKD_MODE=-1

SKD_PUSH_THREADS=2

FAST_NODES="0"
SLOW_NODES="1"


# ENABLE_NTIER=0

DISABLE_MIGRATION=0

# ensure BASE_DIR is set
if [ -z "$BASE_DIR" ]; then
    echo "Setting BASE_DIR to $BASE_DIR"
    BASE_DIR=$(dirname $(realpath $0))/../
fi

PS_HOME_DIR="${BASE_DIR}/perf_scripts"
ILP_HOME_DIR="${BASE_DIR}/ilp_server"
MASIM_HOME="${BASE_DIR}/masim_mod"


SKD_OPS_MODE="BASELINE"


export ILP_HOME_DIR
export PS_HOME_DIR
export PERF_BIN
export ENABLE_NTIER
# --------------

