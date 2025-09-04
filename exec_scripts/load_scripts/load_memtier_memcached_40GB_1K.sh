# memtier_benchmark --server=127.0.0.1 --port=11211 --protocol=memcache_text -c 1 -t 60 --data-size=1011 --ratio=1:0 --key-pattern=S:S --key-maximum=41943040 -n 41943040

CMD="memtier_benchmark $(cat exec_scripts/config/memcached/memtier_memcached_gauss_1K.cfg) --key-pattern=S:S --ratio 1:0 -n 41943040 -t 1"
echo "Running command: $CMD"
eval $CMD
