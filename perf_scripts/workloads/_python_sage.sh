source /data/sathvik/tpp-pytorch-extension/env.sh
export LD_LIBRARY_PATH=/data/sathvik/mini-distgnn-integrated/third_party/libxsmm/lib/
export PATH=$PATH:/usr/local/go/bin
export PATH=$PATH:/root/go/bin



BENCH="python"
EXEC_MODE_BENCH="sage"
WORKLOAD_FILE="ogbn-products"
MAIN_BENCH=1
TEST_TIME=1
WORKLOAD_CONFIG="TEST"

export BENCH
export EXEC_MODE_BENCH
export MAIN_BENCH
export WORKLOAD_FILE
export TEST_TIME



export_bench_run() {
    
    BENCH_RUN="python /data/sandeep/git_repo/ntier_exec_scripts/main.py --num-layers 5 --dataset ${WORKLOAD_FILE} --num-epochs ${TEST_TIME}"
    
    export BENCH_RUN
    
}
export -f export_bench_run
export_bench_run
