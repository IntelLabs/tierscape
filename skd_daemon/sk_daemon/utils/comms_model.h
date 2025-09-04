#ifndef COMMS_MODEL_H
#define COMMS_MODEL_H

//  #include <iostream>
#include <vector>
#include <stdint.h>
#include <string>
#include <iostream>

using namespace std;

#define USD_LIMIT_MODE
// #define PERF_LIMIT_MODE


// Ensure either USD_LIMIT_MODE or PERF_LIMIT_MODE is defined
#if defined(USD_LIMIT_MODE) && defined(PERF_LIMIT_MODE)
#error "Both USD_LIMIT_MODE and PERF_LIMIT_MODE are defined"
#elif !defined(USD_LIMIT_MODE) && !defined(PERF_LIMIT_MODE)
#error "Neither USD_LIMIT_MODE nor PERF_LIMIT_MODE is defined"
#endif


#define NTIER_KERNEL

struct TierInfo_Comm {
public:
	int virt_tier_id = 0;
	bool isCPU;
	int backing_store = 0;
	float compression_ratio;

	int tier_cost;
	int tier_latency;
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
#ifdef USD_LIMIT_MODE	
		printf("Performance Loss: %ld\n", final_obejctive_value);
		cout << "Allowed USD limit: " << resource_limit << ". Actual USD: " << total_other_value << endl;
#else
		cout << "TCO: " << final_obejctive_value << endl;
		cout << "Allowed Perf Loss: " << resource_limit << ". Actual Loss: " << total_other_value << endl;
#endif
		cout << "Min TCO: " << min_tco << ". Max TCO: " << max_tco << endl;
		cout << "Min Perf Loss: " << min_perf_loss << ". Max Perf Loss: " << max_perf_loss << endl;

	}
};

#endif