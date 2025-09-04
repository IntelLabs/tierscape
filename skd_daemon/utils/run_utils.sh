

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
        echo "WARN SKD_MODE is not set to 0, 1 or 2. Exiting"
        SKD_OPS_MODE="BASELINE"
        
    fi

    echo "SKD_OPS_MODE: ${SKD_OPS_MODE}"
    export SKD_OPS_MODE
    
}

export -f VALIDATE_AND_EXPORT_SKD_SETTING