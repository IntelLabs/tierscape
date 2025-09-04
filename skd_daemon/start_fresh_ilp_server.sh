# ensure REMOTE_MODE is set
if [ -z "${REMOTE_MODE}" ]; then
    echo "SKD: REMOTE_MODE is not set, using default (local)"
    REMOTE_MODE=0
fi





# source /tmp/skd_env.sh

# ensure ILP_HOME_DIR is set
if [ -z "${ILP_HOME_DIR}" ]; then
    echo "SKD: FATAL ILP_HOME_DIR is not set"
    exit 1
fi


remote_command() {
    local command_to_run="$@"
    ssh -T sandeep@${REMOTE_ILP_SERVER_NAME} "$command_to_run"
}

remote_tmux_command() {
    local command_to_run="$@"
    echo "Running command: $command_to_run"

    remote_command tmux kill-session -t skdilp
    sleep .2
    # ssh -tt sandeep@${REMOTE_ILP_SERVER_NAME} "tmux new-session -s skdilp -d "
    
    remote_command tmux new-session -s skdilp -d
    remote_command tmux send-keys -t skdilp \"${command_to_run}\" ENTER
    
    
}

tmux_command() {
    local command_to_run="$*"
    echo "Running command: $command_to_run"
    
    # Create session if not exists
    tmux has-session -t skdilp 2>/dev/null || tmux new-session -s skdilp -d
    
    # Send the entire command as a quoted string
    tmux send-keys -t skdilp "$command_to_run" C-m
}

# ensure atleast one argument
if [ $# -lt 1 ]; then
    echo "Usage: $0 <TCO_SAVE_PERCENT>"
    exit 1
fi

# if MAIN_LOG_DIR is not set
if [ -z "${MAIN_LOG_DIR}" ]; then
    echo "SKD: MAIN_LOG_DIR is not set, using default"
    MAIN_LOG_DIR="/tmp"
fi

echo $MAIN_LOG_DIR
TCO_SAVE_PERCENT=$1

echo "Starting ILP server with TCO_SAVE_PERCENT: $TCO_SAVE_PERCENT REMOTE_MODE: $REMOTE_MODE"

echo "Cleaning skdilpserver "
tmux kill-session -t skdilp
for ilp_pid in $(pidof skd_ilp_server); do
    echo "Killing Tracker with PID "${ilp_pid}
    sudo kill -SIGINT ${ilp_pid}
    sudo kill -9 ${ilp_pid}
done


ILP_PERF_EVENTS="cache-misses,cpu-cycles,instructions,cpu-clock,major-faults,minor-faults,page-faults,task-clock"
echo "ILP_PERF_EVENTS: $ILP_PERF_EVENTS"

# restart the ILP server on the ${REMOTE_ILP_SERVER_NAME} machine
echo "Starting the ILP server with ${TCO_SAVE_PERCENT}"

if [ ${REMOTE_MODE} -eq 1 ]; then
    echo "Starting ILP server on remote machine"
    remote_tmux_command  ${ILP_HOME_DIR}/build/skd_ilp_server ${TCO_SAVE_PERCENT}
else
    echo "Starting ILP server on local machine"
    # tmux_command ${PERF_BIN} stat -x, -o ${ILP_PERF_STATS} -e ${ILP_PERF_EVENTS} --  ${ILP_HOME_DIR}/build/skd_ilp_server ${TCO_SAVE_PERCENT} |tee -a ${ILP_LOG}
    tmux_command "${PERF_BIN} stat -x, -o ${ILP_PERF_STATS} -e ${ILP_PERF_EVENTS} -- bash -c '${ILP_HOME_DIR}/build/skd_ilp_server ${TCO_SAVE_PERCENT} 2>&1' | tee -a ${ILP_LOG}"

    
fi

