

function VALIDATE_AND_EXPORT_SKD_SETTING(){
    
    export SKD_PERF_EVENTS
    export SKD_FREQ
    export SKD_WIN_SIZE
    export SKD_HOTNESS_THRESHOLD
    export SKD_MODE
    export EXP_NAME
    export PS_HOME_DIR
    
    if [ ${SKD_MODE} -eq 0 ]; then
        SKD_OPS_MODE="HEMEM"
        
    elif [ ${SKD_MODE} -eq 1 ]; then
        SKD_OPS_MODE="ILP"
        
    elif [ ${SKD_MODE} -eq 2 ]; then
        SKD_OPS_MODE="WATERFALL"
        
    else
        SKD_OPS_MODE="BASELINE"
        echo "WARN SKD_MODE is not set to 0, 1 or 2. Its ${SKD_MODE}. Disabling SKD"
        ENABLE_SKD=0
        
    fi

    echo "SKD_OPS_MODE: ${SKD_OPS_MODE}"
    export SKD_OPS_MODE

    # ensure AGG_MODE is set and is 0 1 or 2
    if [ -z ${AGG_MODE} ]; then
        echo "AGG_MODE is not set. Setting it to 0 (conservative)"
        AGG_MODE=0
    elif [ ${AGG_MODE} -ne 0 ] && [ ${AGG_MODE} -ne 1 ] && [ ${AGG_MODE} -ne 2 ]; then
        echo "AGG_MODE is not set to 0, 1 or 2. Its ${AGG_MODE}. Setting it to 0 (conservative)"
        AGG_MODE=0
    fi
    
}

export -f VALIDATE_AND_EXPORT_SKD_SETTING