PROFILE_PLOT_FIGS() {
    
    
    echo "Plotting the data"
    # ensure SKD_HOME_DIR and MAIN_LOG_DIR are set
    if [ -z ${SKD_HOME_DIR} ]; then
        echo "Error: SKD_HOME_DIR is not set"
        return 1
    fi
    if [ -z ${MAIN_LOG_DIR} ]; then
        echo "Error: MAIN_LOG_DIR is not set"
        return 1
    fi
    
    # check if plot_all.sh is present
    if [  -e ${SKD_HOME_DIR}/plot_all.sh ]; then
        echo "Found at realpath $(realpath ${SKD_HOME_DIR}/plot_all.sh)"
        # bash plot_all.sh ${MAIN_LOG_DIR} $1
        bash ${SKD_HOME_DIR}/plot_all.sh ${MAIN_LOG_DIR} $1
    else
        
        echo "Error: plot_all.sh not found in SKD_HOME_DIR"
        return 1
    fi
}

export -f PROFILE_PLOT_FIGS