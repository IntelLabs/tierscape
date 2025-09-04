HOME_DIR=$(dirname $(realpath $0))
export HOME_DIR
YCSB_HOME="/data/sandeep/git_repo/workload_generator/YCSB"

# ensure one argument is given
if [ $# -ne 1 ]; then
	echo "Usage: $0 <workload_file>"
	exit 1
fi


WORKLOAD_FILE=$1
THREADS=$(nproc)
# THREADS=2

${YCSB_HOME}/bin/ycsb.sh load memcached -s -P ${WORKLOAD_FILE} -p threadcount=$THREADS -p "memcached.hosts=127.0.0.1"
