if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

# get dir of the script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
export SCRIPT_DIR

TMP_FILE=${SCRIPT_DIR}/tmp

generate_file(){ 
	TOT_FILE=$1
	FMT_FILE=$2

	echo $TOT_FILE $FMT_FILE

	echo > ${FMT_FILE}
	
	for l in $(cat ${TOT_FILE})
	do
		echo -n "."
		val=$(cat ${TMP_FILE}|grep $l|wc -l)
		if [ $val -ne 0 ];then
			# echo $1

			echo -n $l"," >> ${FMT_FILE}
		fi
	done

	truncate -s-1 ${FMT_FILE}
	echo "" >> ${FMT_FILE}
	cat ${FMT_FILE}

}

${PERF_BIN} list > ${TMP_FILE}

generate_file ${SCRIPT_DIR}/perf-all-tot ${SCRIPT_DIR}/perf-all-fmt
generate_file ${SCRIPT_DIR}/perf-trend-tot ${SCRIPT_DIR}/perf-trend-fmt

rm ${TMP_FILE}
