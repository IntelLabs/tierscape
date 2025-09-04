#ifndef COMMS_MODEL_H
#define COMMS_MODEL_H

//  #include <iostream>
#include <vector>
#include <stdint.h>
#include <string>
#include <iostream>

#include "../utils/utils.h"

using namespace std;


#define NTIER_KERNEL

static int vid=0;
struct TierInfo_Comm {
public:
	int virt_tier_id = 0;
	bool isCPU;
	int backing_store = 0;
	float compression_ratio;
	

	int tier_cost;
	int tier_latency;

	// add an empty constructor
	TierInfo_Comm() {}

	// add a constructor
	TierInfo_Comm( float compression_ratio, int tier_cost, int tier_latency) :
		virt_tier_id(vid++), isCPU(true), backing_store(-1), compression_ratio(compression_ratio), tier_cost(tier_cost), tier_latency(tier_latency) {}
};


class DataPacket {
public:
	uint64_t final_obejctive_value;
	uint64_t resource_limit;
	uint64_t total_other_value;

	uint64_t min_tco, max_tco;
	uint64_t min_perf_loss, max_perf_loss;
	bool failed = false;


	void toString()
	{
		if (failed)
		{
			cout << "WARN: ILP Failed" << endl;
			return;
		}
		float perf_close_to_min = ((final_obejctive_value - (float)min_perf_loss) / (max_perf_loss - (float)min_perf_loss))*100;
		float tco_close_to_min = ((total_other_value - (float)min_tco) / (max_tco - (float)min_tco))*100;

		pr_info("Achieved TCO: %ld (%.2f) | TCO (min/max): %ld/%ld\n", total_other_value, tco_close_to_min, min_tco, max_tco);
		pr_info("Achieved Perf Loss: %ld (%.2f)  | Perf Loss (min/max): %ld/%ld\n", final_obejctive_value, perf_close_to_min, min_perf_loss, max_perf_loss);

		// 	USD Limit: %ld | Actual USD: %ld\n", final_obejctive_value, resource_limit, total_other_value);
		// printf("TCO (min/max): %ld/%ld | 
		// min_tco, max_tco
		

	}
};

#endif