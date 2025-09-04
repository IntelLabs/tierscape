
CMD="memtier_benchmark $(cat exec_scripts/config/memcached/memtier_memcached_gauss_4K.cfg) --key-pattern=S:S --ratio 1:0 -n 1048576 -t 1"
echo "Running command: $CMD"
eval $CMD