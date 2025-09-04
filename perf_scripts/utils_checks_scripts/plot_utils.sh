PROFILE_PLOT_FIGS() {
    
    
    echo "Plotting the data"
    
    # check MAIN_LOG_DIR
    if [ -z ${MAIN_LOG_DIR} ]; then
        echo "Error: MAIN_LOG_DIR is not set"
        return 1
    fi
    if [ ! -d ${MAIN_LOG_DIR} ]; then
        echo "Error: MAIN_LOG_DIR does not exist"
        return 1
    fi
    
    # check if plot_all.sh is present
    if [  -e plot_all.sh ]; then
        echo "Found at realpath $(realpath plot_all.sh)"
        bash plot_all.sh ${MAIN_LOG_DIR} $1
    else
        # ensure PS_HOME_DIR is set and exists
        if [ -z ${PS_HOME_DIR} ]; then
            echo "Error: PS_HOME_DIR is not set"
            return 1
        fi
        if [ ! -d ${PS_HOME_DIR} ]; then
            echo "Error: PS_HOME_DIR does not exist"
            return 1
        fi
        # check if plot_all.sh is present in PS_HOME_DIR
        if [ ! -e ${PS_HOME_DIR}/plot_all.sh ]; then
            echo "Error: plot_all.sh not found in PS_HOME_DIR"
            return 1
        fi
        
        bash ${PS_HOME_DIR}/plot_all.sh ${MAIN_LOG_DIR} $1
    fi
}

export -f PROFILE_PLOT_FIGS