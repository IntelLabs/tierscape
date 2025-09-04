# ensure root
if [ $EUID -ne 0 ]; then
	echo "Error: This script must be run as root" 2>&1 | tee -a $SCRIPT_LOG
	exit 1
fi

ENV_FILE="/tmp/runall_env.sh"

echo "" > ${ENV_FILE}


# echo DISABLE_MIGRATION=1 >> ${ENV_FILE}
# sudo bash skd_profile_driver.sh workloads/_memtier_memcached_ops.sh


echo "" > ${ENV_FILE}

for remote_mode in 0 1; do
	for hotness_th in .5 .9; do

		echo "SKD_HOTNESS_THRESHOLD=${hotness_th}" >> ${ENV_FILE}
		echo "REMOTE_MODE=${remote_mode}" >> ${ENV_FILE}

		sudo bash skd_profile_driver.sh workloads/_memtier_memcached_ops.sh
	done
done






