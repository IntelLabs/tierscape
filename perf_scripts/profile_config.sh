#!/bin/bash

config_list="
OMP_NUM_THREADS=20
TMP_DIR=/tmp/sk
"

perf_config_list="
TREND_SLEEP_DURATION=2
PEBS_HOTNESS=0
PEBS_C_FREQ=100000
AUTONUMA_MODE=0
"

TREND_DIR="${PS_HOME_DIR}/scripts"

    export TREND_DIR

    # if exp_name is not defined
    if [ -z ${EXP_NAME} ]; then
        EXP_NAME="exp_test"
        export EXP_NAME
    fi

    if [ -z ${OPS_MODE} ]; then
        OPS_MODE="BASELINE"
        export OPS_MODE
    fi


    # =============================

    for line in $perf_config_list; do
        export "$line"
    done

    export PERF_TIMER
    # ==================


    for line in $config_list; do
        export "$line"
    done
    # ================


# ================== Dependent variables. =============================
PERF_TIMER=$(echo $TREND_SLEEP_DURATION*1000 | bc)
QUIT_FILE="${TMP_DIR}/alloctest-bench.quit"
READY_FILE="${TMP_DIR}/alloctest-bench.ready"

export PERF_TIMER
export QUIT_FILE
export READY_FILE