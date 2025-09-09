# events="cpu-cycles instructions cpu-clock"
events="cache-misses cpu-cycles instructions cpu-clock major-faults minor-faults page-faults task-clock"

# ilp_perf_stat="/data/sandeep/git_repo/parl_memory_perf_scripts/evaluation/eval_masim/exp_test/default/perflog-ILP-.1-20250413-205345/ilp_perf_stat"
# tot_perf_stat="/data/sandeep/git_repo/parl_memory_perf_scripts/evaluation/eval_masim/exp_test/default/perflog-ILP-.1-20250413-205345/perflog-perf_final_stats"

# ensure two files passed
if [ $# -ne 2 ]; then
	echo "Usage: $0 <ilp_perf_stat> <tot_perf_stat>"
	exit 1
fi

tot_perf_stat=$1
ilp_perf_stat=$2

# ensure both the file exists
if [ ! -f $tot_perf_stat ]; then
	echo "Error: $tot_perf_stat does not exist"
	exit 1
fi
if [ ! -f $ilp_perf_stat ]; then
	echo "Error: $ilp_perf_stat does not exist"
	exit 1
fi

for evt in $events
do
	echo -n "Event: $evt "
	# Extract the relevant lines from ilp_perf_stat
	ilp_count=$(grep "$evt" "$ilp_perf_stat" | awk -F',' '{print $1}' )
	# Extract the relevant lines from tot_perf_stat
	tot_count=$(grep "$evt" "$tot_perf_stat" | awk -F',' '{print $1}' )

	# float division
	

	# ratio=$(echo "scale=6; $ilp_count / $tot_count" | bc)
	# percentage
	ratio=$(echo "scale=4; $ilp_count / $tot_count * 100" | bc)
	echo "ratio: ${ratio}% ($ilp_count / $tot_count)"
	
	# # Combine the two files into one
	# paste "ilp_$evt.txt" "tot_$evt.txt" > "${evt}_combined.txt"
	
	# # Clean up intermediate files
	# rm "ilp_$evt.txt" "tot_$evt.txt"
done