# memtier_benchmark --server=127.0.0.1 --port=11211 --protocol=memcache_text -c 1 -t 64 -n 10485760 --data-size=4083 --ratio=1:0 --key-pattern=S:S --key-maximum=10485760

CMD="memtier_benchmark $(cat exec_scripts/config/memcached/memtier_memcached_gauss_4K.cfg) --key-pattern=S:S --ratio 1:0 -n 10485760 -t 1"
echo "Running command: $CMD"
eval $CMD