HOME_DIR=$(dirname $(realpath $0))
export HOME_DIR
YCSB_HOME="/data/sandeep/git_repo/YCSB"

# ensure one argument is given
if [ $# -ne 1 ]; then
	echo "Usage: $0 <workload_file>"
	exit 1
fi


WORKLOAD_FILE=$1
THREADS=50

 ${YCSB_HOME}/bin/ycsb.sh load redis -s -P ${WORKLOAD_FILE} -p redis.host=127.0.0.1 -p redis.port=6379 -p threadcount=$THREADS
