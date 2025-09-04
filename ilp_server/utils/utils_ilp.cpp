#include <algorithm>

#include "skd_ilp.h"
#include "utils_ilp.h"



uint64_t SKD_ILP::get_max_TCO()
{
	if (!init_done) {
		printf("init not done\n");
		return 0;
	}

	float max_cost = 0;
	int max_idx = -1;
	for (int i = 0; i < num_tiers; i++) {
		float curr_cost = tier_costs[i] * tier_com[i];
		if (curr_cost > max_cost || max_idx == -1) {
			max_cost = curr_cost;
			max_idx = i;
		}
	}

	uint64_t lease_cost = 0;
	for (int i = 0; i < num_regions; i++) {
		lease_cost += 
		(uint64_t)(
		// grp_regions_size_mb[i]
			INIT_REGION_MB * MULTIPLIER_FACTOR 
			 * tier_costs[max_idx] * tier_com[max_idx]);
	}
	if (lease_cost == 0) {
		fprintf(stderr, "WARN: lease_cost max_cost is 0\n");
		// throw std::runtime_error("max_cost is 0");
	}
	return lease_cost;
}

uint64_t SKD_ILP::get_min_TCO()
{
	if (!init_done) {
		printf("init not done\n");
		return 0;
	}

	float min_cost = 9999;
	int min_idx = -1;

	for (int i = 0; i < num_tiers; i++) {
		float curr_cost = tier_costs[i] * tier_com[i];
		if (curr_cost < min_cost || min_idx == -1) {
			min_cost = curr_cost;
			min_idx = i;
		}
	}

	uint64_t lease_cost = 0;
	for (int i = 0; i < num_regions; i++) {
		lease_cost += 
		(uint64_t)(
		// grp_regions_size_mb[i] 
			INIT_REGION_MB * MULTIPLIER_FACTOR 
			* tier_costs[min_idx] * tier_com[min_idx]);
	}
	return lease_cost;
}

uint64_t SKD_ILP::get_max_perf_loss()
{
	if (!init_done) {
		printf("init not done\n");
		return 0;
	}

	int max_lat = 0;
	int max_idx = -1;
	for (int i = 0; i < num_tiers; i++) {
		if (tier_lats[i] > max_lat || max_idx == -1) {
			max_lat = tier_lats[i];
			max_idx = i;
		}
	}


	uint64_t max_perf_loss = 0;
	for (int i = 0; i < num_regions; i++) {

		if (isZero(grp_hotness[i]) == false && grp_hotness[i] < 98985ul && grp_hotness[i] > 0ul) {
			max_perf_loss += (uint64_t)((grp_hotness[i] * tier_lats[max_idx]));
		}
	}
	if (max_perf_loss == 0) {
		fprintf(stderr, "WARN: max_perf_loss is 0\n");
	}
	return max_perf_loss;
}

uint64_t SKD_ILP::get_min_perf_loss()
{
	if (!init_done) {
		printf("init not done\n");
		return 0;
	}

	int min_lat = 9999;
	int min_idx = -1;

	for (int i = 0; i < num_tiers; i++) {
		if (tier_lats[i] < min_lat || min_idx == -1) {
			min_lat = tier_lats[i];
			min_idx = i;
		}
	}


	uint64_t min_perf_loss = 0;
	for (int i = 0; i < num_regions; i++) {
		min_perf_loss += (uint64_t)(grp_hotness[i] * tier_lats[min_idx]);
	}
	/* This will be 0. just running loop for the same of any future changes where DRAM latency is increased from 0 */
	return min_perf_loss;
}



bool compareByHotness(const mod_region& a, const mod_region& b) {
	return a.hotness < b.hotness;
}


void printHistogram(std::vector<uint64_t> numbers, uint64_t numBuckets) {
	printf("Printing histograms numbers.size() %lu in buckets of %ld\n", numbers.size(), numBuckets);
	std::vector<int> bucketCounts(numBuckets, 0);


	uint64_t maxElement = *max_element(numbers.begin(), numbers.end());
	uint64_t nint_percentile = *(numbers.begin() + (numbers.size() * 90) / 100);
	printf("90th percentile is %lu and max elem is %lu\n", nint_percentile, maxElement);

	maxElement = nint_percentile;
	if (numBuckets > maxElement) {
		numBuckets = maxElement;
	}

	int range = maxElement / numBuckets;  // Assuming numbers range from 0 to 100
	if (range == 0) {
		fprintf(stderr, "range is 0\n");
		return;
	}

	for (uint64_t number : numbers) {
		uint64_t bucketIndex = number / range;
		if (bucketIndex >= numBuckets) {
			bucketIndex = numBuckets - 1;
			// fprintf(stderr, "bucketIndex %d numBuckets %d number %lu range %d\n", bucketIndex, numBuckets, number, range);
			// continue;
		}
		bucketCounts[bucketIndex]++;
	}


	for (uint64_t i = 0; i < numBuckets; i++) {
		std::cout << i * range << "-" << (i + 1) * range - 1 << ": " << bucketCounts[i] <<"| ";
	}
	std::cout << std::endl;
}

void SKD_ILP::init_data_from_comms(vector<uint64_t > regions, vector<TierInfo_Comm*> tiers)
{
	num_regions = regions.size();
	int orig_num_regions = num_regions;
	num_tiers = tiers.size();

	// grp_regions_size_mb.clear();
	grp_hotness.clear();
	tier_costs.clear();
	tier_lats.clear();
	tier_com.clear();
	// ====================

	mod_regions.clear();

	for (int i = 0; i < orig_num_regions; i++) {
		mod_regions.push_back(mod_region(regions[i], i));
	}
	std::sort(mod_regions.begin(), mod_regions.end(), compareByHotness);
	
	fprintf(stderr, " -------------\nUsing a mult factor of %d init mb size is %d\n", MULTIPLIER_FACTOR, INIT_REGION_MB);
	for (int i = 0; i < orig_num_regions; i++) {
		/* the region id where I am placed */
		mod_regions[i].group_id = i / MULTIPLIER_FACTOR;
	}

	for (int i = 0;i < orig_num_regions;i += MULTIPLIER_FACTOR) {
		
		int mod_mult_factor = ((i + MULTIPLIER_FACTOR) > orig_num_regions) ? orig_num_regions - i : MULTIPLIER_FACTOR; // return mulit-factor, in the end return whats l;eft.
		// grp_regions_size_mb.push_back((INIT_REGION_MB * mod_mult_factor)); //generally INIT_REGION_MB*MULTIPLIER_FACTOR

		int max_hotness = 0;
		for (int j = i; j < i + mod_mult_factor; j++) {
			if (mod_regions[j].hotness > max_hotness) {
				max_hotness = mod_regions[j].hotness;
			}
		}
		max_hotness = (max_hotness ==0) ? 1 : max_hotness;
		grp_hotness.push_back(max_hotness);
	}

	num_regions = grp_hotness.size();
	merged = true;

	// ======================================

	for (int i = 0; i < num_tiers; i++) {
		tier_costs.push_back(tiers[i]->tier_cost);
		tier_lats.push_back(tiers[i]->tier_latency);
		tier_com.push_back(tiers[i]->compression_ratio);
		// printf("Read Tier ID: %d tier_latency: %d compression_ratio: %f cost %d \n", i, tier_lats[i], tier_com[i], tier_costs[i]);
		fprintf(stderr,"%d [Lat: %d Com: %.2f Cost: %d] ", i, tier_lats[i], tier_com[i], tier_costs[i]);
	}
	fprintf(stderr, "\n");

	printf("original num_regions %d new num_regions %d mod_regions.size() %lu num_tiers %d\n", orig_num_regions, num_regions, mod_regions.size(), num_tiers);
	printHistogram(grp_hotness, 10);
	printf("-------------------\n");

	init_done = 1;
}



