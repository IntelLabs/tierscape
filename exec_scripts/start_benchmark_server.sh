if [ $# -lt 1 ]
then
    echo "Need an argument. Benchmark name "
    exit 1
else
    BENCH=$1
fi

memcached_threads=10



kill_benchmark(){
    echo "Killing benchmarks"
    sudo systemctl stop memcached
    sudo systemctl disable memcached
    pkill -9 memcached
    
    pkill -9 redis
    pkill -9 redis-server
    sleep 1
}

# ======================
# ======= REDIS ========
# ======================
# if BENCH contains redis


function start_redis(){
    kill_benchmark
    if [ ${BENCH} == "redis_optane" ]; then
        echo "Starting Redis Optane " ${BENCH}
        numactl -i ${OPTANE_NODES} -- /usr/local/bin/redis ${REDIS_CONF}
    elif [ ${BENCH} == "redis_dram" ]; then
        echo "Starting Redis DRAM " ${BENCH}
        CMD="numactl -i ${DRAM_NODES} -- /usr/local/bin/redis ${REDIS_CONF}"
        echo $CMD
        $CMD
    elif [ ${BENCH} == "redis" ]; then
        echo "Starting Redis " ${BENCH}
        /usr/local/bin/redis ${REDIS_CONF}
    fi
}

# ======================
# ======= MEMCACHED ========
# ======================

function start_memcached(){
    # -I, --max-item-size=<num> adjusts max item size
    # -p,  port
    # -u username
    # -m, --memory-limit=<num>  item memory in megabytes (default: 64)
    # -M, --disable-evictions   return error on memory exhausted instead of evicting
    # -d, --daemon              run as a daemon
    # -t, --threads=<num>       number of threads to use (default: 4)
    
    MEMCACHED_ARGS="-I 5m -p 11211 -u memcached  -m 5120000 -M -d -t ${memcached_threads}"
    

        kill_benchmark
        
        echo "Starting Memcached with 5120000 MB"
        cmd="memcached ${MEMCACHED_ARGS}"
        echo $cmd
        $cmd
    sleep 1
}


if [[ "$BENCH" == *"redis"* ]];
then
    echo "Starting REDIS"
    start_redis
    BENCHMARK_PID=$(pidof redis)
    while [ -z $BENCHMARK_PID ]
    do
        start_redis
        BENCHMARK_PID=$(pidof redis)
        echo "Redis PID is $BENCHMARK_PID"
        sleep .1
    done
elif [[ "$BENCH" == *"memcached"* ]];
then
    echo "Starting Memcached"
    start_memcached
    BENCHMARK_PID=$(pidof memcached)
    while [ -z $BENCHMARK_PID ]
    do
        start_memcached
        BENCHMARK_PID=$(pidof memcached)
        echo "Memcached PID is $BENCHMARK_PID"
        sleep .1
    done
else
    echo "Unknown benchmark"
    exit 1
fi

echo "Benchmark PID is $BENCHMARK_PID"
