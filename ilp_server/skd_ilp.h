#ifndef SKD_ILP_H
#define SKD_ILP_H

#include <iostream>
#include <memory>
#include <numeric>
#include <vector>
#include <time.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <future>

#include "absl/strings/str_format.h"
#include "ortools/linear_solver/linear_expr.h"
#include "ortools/linear_solver/linear_solver.h"

#include "model/comms_model.h"

#define DST_DRAM -1
#define DST_OPTANE -2
#define DST_IGNORE -3



using namespace operations_research;
using namespace std;

#include <type_traits>

template <typename T> bool isZero(const T& value)
{
	if constexpr (std::is_floating_point_v<T>) {
		return value == 0.0;
	}
	else {
		return value == 0;
	}
}

struct mod_region {
	int hotness;
	int id;
	int group_id;

	mod_region(int hotness, int id) : hotness(hotness), id(id), group_id(-1) {}
};

class SKD_ILP {
public:
	vector<int> tier_costs;
	vector<int> tier_lats;
	vector<float> tier_com;
	// vector<int> grp_regions_size_mb;
	vector<uint64_t> grp_hotness;

	int num_regions = 10;
	int num_tiers;
	int init_done = 0;

	bool merged = false;

	uint64_t resource_limit = 200;
	float tco_percent = 0.5;


	vector<int> dst_tiers;
	vector<mod_region> mod_regions;


	uint64_t get_max_TCO();
	uint64_t get_min_TCO();

	uint64_t get_max_perf_loss();
	uint64_t get_min_perf_loss();

	void init_data_from_comms(vector<uint64_t > regions, vector<TierInfo_Comm*> tiers);

	DataPacket* set_solver_things_and_solve();

	void prepare_data_packet(std::vector<std::vector<operations_research::MPVariable*>>& pos_ident, DataPacket*& dp, operations_research::MPObjective* const objective);


	void set_resource_limit(float per_limit)
	{
		// uint64_t resource_limit;

		uint64_t max_tco = get_max_TCO();
		uint64_t min_tco = get_min_TCO();

		resource_limit = min_tco + (max_tco - min_tco) * per_limit;
		tco_percent = per_limit;
		// return ;
	}

	float get_tco_percent()
	{
		return tco_percent;
	}
	uint64_t get_resource_limit()
	{
		return resource_limit;
	}
};

#endif