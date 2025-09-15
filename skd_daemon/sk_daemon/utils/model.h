#ifndef MODEL_H
#define MODEL_H

#pragma once

#include <argp.h>
#include <err.h>
#include <fcntl.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

// #define CONFIG_HOT_AVG_MODE

// TOOD enable this.
// #define CONFIG_DISTRIBUT_FAULTS

/* autoupdated on running make setup from the root dir as per skd_config.sh */
#define FAST_NODE 0
#define SLOW_NODE 1


#define MAX_ITMES 5
#define PAGESIZE 4096

#define REMOTE_ILP_SERVER "10.223.93.197"
#define LOCAL_ILP_SERVER "127.0.0.1"

#define CHAR_FILE_LEN 500

#include "config.h"
#include "skd_error.h"

#define PAGE_SHIFT 12
#define PAGE_SIZE (1UL << PAGE_SHIFT)
#define PAGE_MASK (~(PAGE_SIZE - 1))

// #define PAGE_ALIGN(addr)        (((addr)+PAGE_SIZE-1)&PAGE_MASK)
#define PAGE_ALIGN(addr) (((addr)) & PAGE_MASK)

#include <atomic>
#include <unordered_set>

// #define RESET   "\e[0m"
// #define RED     "\e[91m"      /* Red */
// #define YELLOW  "\e[933m"      /* Yellow */

// #define BLACK   "\033[30m"      /* Black */
// #define GREEN   "\033[32m"      /* Green */
// #define BLUE    "\033[34m"      /* Blue */
// #define MAGENTA "\033[35m"      /* Magenta */
// #define CYAN    "\033[36m"      /* Cyan */
// #define WHITE   "\033[37m"      /* White */
// #define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
// #define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
// #define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
// #define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
// #define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
// #define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
// #define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
// #define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

// Helper macros to concatenate line number safely
#define CONCAT_IMPL(x, y) x##y
#define CONCAT(x, y) CONCAT_IMPL(x, y)

// Final macro
#define WARN_ONCE(message)                                               \
	static std::atomic<bool> CONCAT(warned_once_, __LINE__) = false; \
	if (!CONCAT(warned_once_, __LINE__).exchange(true)) {            \
		std::cerr << "WARNING: " << message << std::endl;        \
	}

#define pr_info_highlight(...)                          \
	fprintf(stderr, "\n**********\n-------INFO: "); \
	fprintf(stderr, __VA_ARGS__);                   \
	fprintf(stderr, "*************\n");
// fprintf(stdout, "%s:%s:%d: ", __FILE__, __FUNCTION__, __LINE__);

#define pr_debug(...)
/*
#define pr_debug(...) \
    fprintf(stderr, " DEBUG: "); \
    fprintf(stderr, __VA_ARGS__);
 */

#define pr_info(...)               \
	fprintf(stderr, "INFO: "); \
	fprintf(stderr, __VA_ARGS__);

#define pr_warn(...)                      \
	fprintf(stderr, "-------WARN: "); \
	fprintf(stderr, __VA_ARGS__);

#define pr_err(...)                                                                         \
	fprintf(stderr, "==== ERROR at %s:%s:%d Msg:\n", __FILE__, __FUNCTION__, __LINE__); \
	fprintf(stderr, __VA_ARGS__);                                                       \
	// fprintf(stderr, "\n");

//

/* #define pr_debug(...) \
    fprintf(stderr, "%s:%d: ", __FUNCTION__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);
    // fprintf(stderr, "\n"); */

enum SKD_MODE {
	HEMEM_MODE,
	NTIER_FIXED_ILP,
	NTIER_FIXED_WATERFALL,
};

#define IN_DRAM 100
#define IN_OPTANE 200
#define LEAVE_IT 300

// #define PHY_DRAM_TIER_ID -1
// #define PHY_OPTANE_TIER_ID -2

// #define PHY_IGNORE -3

// #define DST_INVALID -4
// #define GOOGLE_TIER 3

// these corresponds to NUMA node
// #define VIRT_DRAM_TIER_ID 1
// #define VIRT_OPTANE_TIER_ID 2

// #define FIST_TIER_AFTER_DRAM VIRT_OPTANE_TIER_ID
// #define HOTEST_TIER_VRT_ID VIRT_DRAM_TIER_ID

// #define DRAM_NUMA_NODE 1
// #define OPTANE_NUMA_NODE 2

// extern const char *AGG_MODES_STR[];
extern SKD_MODE skd_mode;
struct REGION_SKD_BASE {

    public:
	uint64_t start_address = 0;
	int curr_virt_tier = FAST_NODE; //
	int dst_virt_tier = -1;	 //
};

// static bool is_printed_all_valid = false;

#define COMPRESSED_TIERS_BASE 10

enum MEM_TYPE {
	DRAM,
	OPTANE,
	CXL,
	HBM,
	COMPRESSED
};

const char *get_mem_type_string(MEM_TYPE type);

class TierInfo {
	// int phy_tier_id = 0;
    private:
	int virt_tier_id = 0;
	static const int dram_tier_cost = 10;
	static const int optane_tier_cost = 3;

    public:
	// string type; /* Same as zpool */
	MEM_TYPE mem_type;
	string pool_manager;
	string compressor;
	bool isCPU;

	uint64_t nr_compressed_size = 0;
	int backing_store = 0;
	uint64_t nr_pages = 0;
	uint64_t last_seen_faults = 0;
	uint64_t faults = 0;

	float compression_ratio;

	int tier_cost;
	int tier_latency;

	TierInfo(int16_t _virt_id, MEM_TYPE _mem_type, int _backing_store, int _tier_latency) {
		virt_tier_id = _virt_id;
		mem_type = _mem_type;
		pool_manager = "na";
		compressor = "na";
		backing_store = _backing_store;
		isCPU = true;

		tier_latency = _tier_latency;
		compression_ratio = 1;

		if (backing_store == FAST_NODE) {
			tier_cost = dram_tier_cost;
		} else {
			tier_cost = optane_tier_cost;
		}
	}

	TierInfo(int16_t _virt_id, string _pool_manager, string _compressor, int _backing_store, bool _is_CPU, int _tier_latency) {

		virt_tier_id = _virt_id;
		mem_type = COMPRESSED;
		pool_manager = _pool_manager;
		compressor = _compressor;
		backing_store = _backing_store;
		isCPU = _is_CPU;
		if (backing_store == FAST_NODE) {
			tier_cost = dram_tier_cost;
		} else {
			tier_cost = optane_tier_cost;
		}

		tier_latency = _tier_latency;

		// /* we need to be optimistic while setting the compression_ratio */
		if (compressor == "zsmalloc") {
			compression_ratio = .2;
		} else if (compressor == "zstd") {
			compression_ratio = .5;
		} else {
			compression_ratio = .8;
		}
	}

	bool isCompressed() {
		return mem_type == COMPRESSED;
	}

	int get_virt_tier_id() {
		return virt_tier_id;
	}

	void set_virt_tier_id(int id) {
		virt_tier_id = id;
	}
};

extern int slow_tier;

class TIERS_INFO {
    private:
	TIERS_INFO();

    public:
	vector<TierInfo *> tiers;

	static TIERS_INFO *get_instance() {
		static TIERS_INFO instance;
		return &instance;
	}

	int get_virt_id_from_idx(int idx) {
		if (idx < 0 || idx >= (int)tiers.size()) {
			pr_warn("Invalid index %d\n", idx);
			return -1;
		}
		return tiers[idx]->get_virt_tier_id();
	}

	int get_idx_from_virtid(int virt_tier) {
		for (int i = 0; i < (int)tiers.size(); i++) {
			if (tiers[i]->get_virt_tier_id() == virt_tier) {
				return i;
			}
		}
		return -1;
	}

	int get_next_tier_virt_id(int virt_tier) {
		if (virt_tier < 0)
			return tiers[0]->get_virt_tier_id();

		int idx = get_idx_from_virtid(virt_tier);
		if (idx == -1) {
			return tiers[0]->get_virt_tier_id();
		}

		// next idx
		int next_idx = idx + 1;
		if (next_idx >= (int)tiers.size()) {
			next_idx = tiers.size() - 1;
		}
		return tiers[next_idx]->get_virt_tier_id();
	}

	int get_prev_tier_virt_id(int virt_tier) {
		if (virt_tier < 0)
			return tiers[0]->get_virt_tier_id();
		int idx = get_idx_from_virtid(virt_tier);
		if (idx == -1) {

			WARN_ONCE("Invalid virt_tier get_prev_tier_virt_id\n");
			return tiers[0]->get_virt_tier_id();
		}

		int prev_idx = idx - 1;
		if (prev_idx < 0) {
			prev_idx = 0;
		}
		return tiers[prev_idx]->get_virt_tier_id();
	}
	int get_fast_tier_virt_id() {
		return tiers[0]->get_virt_tier_id();
	}
	int get_slow_tier_virt_id() {
		if (slow_tier == -1)
			return tiers[tiers.size() - 1]->get_virt_tier_id();
		else {
			if (slow_tier >= (int)tiers.size()) {
				pr_warn("Invalid slow_tier %d\n", slow_tier);
				return tiers[tiers.size() - 1]->get_virt_tier_id();
			} else {
				return tiers[slow_tier]->get_virt_tier_id();
			}
		}
	}

	TierInfo *getTierInfofromID(int virt_tier);

	void add_tier_info(TierInfo *tier_info) {
		tiers.push_back(tier_info);
	}

	int get_nr_tiers() {
		return tiers.size();
	}
};

// Cleaner alias
inline TIERS_INFO *TINFO = TIERS_INFO::get_instance();

struct REGION_SKD : public REGION_SKD_BASE {
    private:
	uint64_t hotness = 0;
	uint64_t end_address = 0;

    public:
	uint64_t collected_hotness = 0;
#ifdef CONFIG_DISTRIBUT_FAULTS
	uint64_t hotness_from_faults = 0;
#endif

	float percentile_hotness = -1;
	float adjusted_hotness = 0;

	uint64_t age = 0;

	uint64_t size_4k = 0;

	// float time;
	uint64_t region_id;

#ifdef CONFIG_HOT_AVG_MODE
	uint64_t latest_hotness[MAX_ITMES];
#endif

	// int curr_virt_tier {
	//     return TIERS_INFO::get_instance()->get_virt_tier_from_phys_tier(curr_virt_tier);
	// }
	float get_percentile_hotness() {
		if (percentile_hotness == -1) {
			fprintf(stderr, "FATAL: Percentile hotness not set for region %lu\n", region_id);
			exit(1);
		}
		return percentile_hotness;
	}

	REGION_SKD(uint64_t _start, uint64_t _end) {

		start_address = _start;
		end_address = _end;

		hotness = 0;
		size_4k = (end_address - start_address) / PAGESIZE;
		// std = 0;
	}

	REGION_SKD(uint64_t _start, uint64_t _end, uint64_t _hotness) {

		start_address = _start;
		end_address = _end;
		set_hotness(_hotness);

		size_4k = (end_address - start_address) / PAGESIZE;
		// std = 0;
	}

	// REGION_SKD(uint64_t _start, uint64_t _end, uint64_t _hotness, uint64_t _size_4k) {
	//     init_avg_hotness();
	//     start_address = _start;
	//     end_address = _end;
	//     hotness = _hotness;
	//     size_4k = _size_4k;
	//     // std = 0;
	// }

#ifdef CONFIG_HOT_AVG_MODE
	int latest_index = -1;
	int valid_items = -1;

	void set_hotness(uint64_t _hotness) {

		if (latest_index == -1)
			latest_index = 0;
		latest_hotness[latest_index] = _hotness;
		valid_items = min(MAX_ITMES, (latest_index + 1)); /* as latest_index is zero based, hence +1 */
		latest_index = (latest_index + 1) % MAX_ITMES;	  /* for the next time */

		uint64_t sum = 0;
		for (int i = 0; i < valid_items; i++) {
			sum += latest_hotness[i];
		}
		hotness = (sum / (uint64_t)valid_items);
		if (valid_items == MAX_ITMES && is_printed_all_valid == false) {
			WARN_ONCE("Avg Hotness Working fine\n");
		}
	}

#else
	// sdfds
	void set_hotness(uint64_t _hotness) {
		hotness = _hotness;
	}
#endif // CONFIG_HOT_AVG_MODE

	uint64_t get_hotness() const {
#ifdef CONFIG_HOT_AVG_MODE
		if (latest_index == -1) {
			// WARN_ONCE("Change me hotness to 0\n")
			// return 1ul;
			return 0ul;
		}
#endif
		return hotness;
	}

	uint64_t get_end_address() {
		return end_address;
	}
	void set_end_address(uint64_t _end_address) {
		end_address = _end_address;
	}
};

struct AddrEntry {
	float addr_time;
	uint64_t addr;

	AddrEntry(float _addr_time, uint64_t _addr) {
		addr_time = _addr_time;
		addr = _addr;
	}
};

struct AddrCountEntry {
	float addr_time;
	uint64_t page_addr;
	uint64_t count;

	inline bool operator<(const AddrCountEntry &rhs) {
		return page_addr < rhs.page_addr;
	}
};

struct address_range {
	uint64_t start_addr;
	uint64_t end_addr;
};

extern char regions_log_file[CHAR_FILE_LEN];

class MAPPING_BASE {
    public:
	virtual int process_regions(vector<REGION_SKD *> *regions) = 0;

	int push_window = 0;
	int disable_print = 0;
	bool disable_migration = false;

	void print_regions_info(vector<REGION_SKD *> regions, int hotness_threshold) {
		// write regions info to file regions_log_file append

		// unused hotness_threshold, compier waring
		(void)hotness_threshold;

		FILE *fp = fopen(regions_log_file, "a+");
		if (fp == NULL) {
			pr_debug("Error opening file %s\n", regions_log_file);
			return;
		}
		fprintf(fp, "INFO total_regions %ld start_address %lu end_address %lu\n", regions.size(), regions[0]->start_address, regions[regions.size() - 1]->get_end_address());
		for (size_t region_id = 0; region_id < regions.size(); region_id++) {
			REGION_SKD *region = regions[region_id];
			fprintf(fp, "%d %ld %d %d %lu\n", push_window, region_id, region->curr_virt_tier, region->dst_virt_tier, region->get_hotness());
		}
		fclose(fp);
		push_window++;
	}
};

struct data_packet {
	int pid;
	int window_size;
	MAPPING_BASE *mapping_logic;
};

#endif