
#include "tier_utils.h"

#include "utils.h"
#include "tier_config.h"
#include <numaif.h>
#include <syscall.h>

std::string exec(const char *cmd);
// static int is_initialized = 0;
extern char migrate_log_file[CHAR_FILE_LEN];

extern pid_t pid;
extern int window_seconds, total_tiers;
extern SKD_MODE skd_mode;

int count_compressed_pools();

// HEMEM_MODE is the TMTS_MODE
static const char *PERF_MODES_STR[] = {
    "HEMEM_MODE",
    "NTIER_FIXED_ILP",
    "NTIER_FIXED_WATERFALL",
};

int is_ntier_kernel() {
	if (access("/tmp/wrong_machine", F_OK) == 0) {
		WARN_ONCE("WRONG MACHINE. No NTIER KERNEL\n")
		return 0;
	}
	return 1;
}

string get_perf_mode_string(int perf_mode_int) {
	if (perf_mode_int < 0) {
		return "NONE";
	}
	return PERF_MODES_STR[perf_mode_int];
}

int count_compressed_pools() {

	int cnt = 0;

	for (size_t i = 0; i < TIERS_INFO::get_instance()->tiers.size(); i++) {
		if (TIERS_INFO::get_instance()->tiers[i]->isCompressed()) {
			cnt++;
		}
	}

	return cnt;
}

bool compare_tiers(TierInfo *a, TierInfo *b) {
	return (a->get_virt_tier_id() < b->get_virt_tier_id());
}

// bool compare_tiers(TierInfo* a, TierInfo* b) {
//     if (a->get_virt_tier_id() < b->get_virt_tier_id()) {
//         return true;
//     }
//     return false;
// }

int OPTANE_PREFERRED = 1;

// Implementation of the configuration helper function
std::vector<TierInfo*> create_configured_tiers() {
	std::vector<TierInfo*> tiers;
	
	for (int i = 0; i < NUM_TIER_CONFIGS; i++) {
		const TierConfigData& config = TIER_CONFIGS[i];
		
		if (config.mem_type==COMPRESSED	) {
			// Create compressed tier using the appropriate constructor
			tiers.push_back(new TierInfo(
				config.virt_id,
				config.pool_manager,
				config.compressor,
				config.backing_store,
				config.is_cpu,
				config.tier_latency
			));
		} else {
			// Create standard tier using the simpler constructor
			tiers.push_back(new TierInfo(
				config.virt_id,
				config.mem_type,
				config.backing_store,
				config.tier_latency
			));
		}
	}
	
	return tiers;
}

TIERS_INFO::TIERS_INFO() {
	pr_debug("\n***********\nINITIALIZE TIERS. THIS SHOULD BE DONE ONLY ONCE.\n**************\n")
	    tiers.clear();

	// Load tiers from shared configuration using TierInfo class
	std::vector<TierInfo*> configured_tiers = create_configured_tiers();
	for (TierInfo* tier : configured_tiers) {
		tiers.insert(tiers.begin(), tier);
	}
	
	pr_debug("Loaded %zu tiers from shared configuration\n", configured_tiers.size());

	/* this is required, as we are inserting in any order */
	std::sort(tiers.begin(), tiers.end(), compare_tiers);

	for (struct TierInfo *t : tiers) {
		fprintf(stderr, "Tier: %d type %s comp %s BS %d compression_ratio %f cost %d latency %d\n", t->get_virt_tier_id(), get_mem_type_string(t->mem_type), t->compressor.c_str(), t->backing_store, t->compression_ratio, t->tier_cost, t->tier_latency);
	}
}

TierInfo *TIERS_INFO::getTierInfofromID(int virt_id) {
	for (int i = 0; i < total_tiers; i++) {
		if (tiers[i]->get_virt_tier_id() == virt_id) {
			return tiers[i];
		}
	}
	return NULL;
}

int fd;
int open_pagemap_file(int pid) {
	char filename[BUFSIZ];
	sprintf(filename, "/proc/%d/pagemap", pid);
	// pr_debug("Opening %s\n", filename);

	if (fd > 0) {
		/* already open */
		return 0;
	}
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		perror("open");
		pr_err("FATAL: Error opening file %s\n", filename);
		exit(1);
	}
	return 0;
}

int close_pagemap_file() {
	if (fd >= 0) {
		close(fd);
	}
	fd = -1;
	return 0;
}

std::mutex is_in_swapmutex; // Declare a global mutex to protect shared data

uint64_t is_in_swap(unsigned long start_address, unsigned long end_address, int pid) {
	std::lock_guard<std::mutex> lock(is_in_swapmutex);

	uint64_t total_pages_in_swap = 0;
	// int ctr = 0;
	if (fd < 0) {
		open_pagemap_file(pid);
		pr_err("ERROR THIS SHOULD NOT HAPPEN\n");
		// return 0;
	}
	for (uint64_t i = start_address; i < end_address; i += 0x10000) {
		uint64_t data;
		uint64_t index = (i / PAGE_SIZE) * sizeof(data);
		if (pread(fd, &data, sizeof(data), index) != sizeof(data)) {
			perror("pread");
			// pr_err("FATAL: Error reading pagemap file fd %d\n", fd);
			// exit(1);
			return 0;
		}
		if (((data >> 62) & 1) == 1ul) {
			total_pages_in_swap++;
		}
	}
	// close(fd);

	return total_pages_in_swap;
}

// returnes pages moved, or negative on error.
int move_to_dram_or_optane(REGION_SKD *curr_region, int pid) {

	int ret = 0;
	uint64_t aligned_start_address, aligned_end_address;
	aligned_start_address = PAGE_ALIGN(curr_region->start_address);
	aligned_end_address = PAGE_ALIGN(curr_region->get_end_address());
	uint64_t page_len_bytes = (aligned_end_address - aligned_start_address);

	int number_of_pages = page_len_bytes / 4096;

	void **addrs = (void **)malloc(sizeof(void *) * number_of_pages);
	int *status = (int *)malloc(sizeof(int) * number_of_pages);
	int *nodes = (int *)malloc(sizeof(int) * number_of_pages);

	int target_node = curr_region->dst_virt_tier;
	// fill adrs
	for (int i = 0; i < number_of_pages; i++) {
		addrs[i] = (void *)(aligned_start_address + i * 4096);
		status[i] = -1;
		nodes[i] = target_node;
	}

	// pr_debug("move_to_dram_or_optane: moving %d pages from %p to %p target_node %d\n", number_of_pages, (void*)aligned_start_address, (void*)aligned_end_address, target_node);

	// status contains location before moving
	int moved = number_of_pages;
	ret = move_pages(pid, number_of_pages, addrs, nodes, status, MPOL_MF_MOVE_ALL);
	if (ret < 0) {
		return ret;
	} else if (ret > 0) {
		pr_debug("move_pages failed to move  %d out of %d pages\n", ret, number_of_pages);
		moved -= ret;
	}

	// for (int i = 0; i < number_of_pages; ++i) {
	//     if (status[i] >= 0 && status[i] != target_node) {
	//         moved++;
	//     }
	// }

	free(addrs);
	free(status);
	free(nodes);

	return moved;
}

int are_tiers_similar(int curr_tier_id, int dist_tier_id) {
	TierInfo *curr_tier = TINFO->getTierInfofromID(curr_tier_id);
	TierInfo *dist_tier = TINFO->getTierInfofromID(dist_tier_id);

	// check type, cost, and lat
	if (curr_tier->mem_type == dist_tier->mem_type && curr_tier->tier_cost == dist_tier->tier_cost && curr_tier->tier_latency == dist_tier->tier_latency) {
		return 1;
	}

	return 0;
}

// returnes pages moved, or negative on error.
int push_a_region(REGION_SKD *curr_region, int pid, int dst_virt_tier) {
	int ret_moved_pages = 0;
    int ret=0;

	// Note: DO not return if the dst node is DRAM or Optane. Call this to bring the data out of zswap. and then move to DRAM or Optane.
	if (dst_virt_tier == -EINVAL || dst_virt_tier < 0) {
		pr_err("WARN: Invalid tier mapping %d\n", dst_virt_tier);
		return -EINVAL;
	}

	uint64_t aligned_start_address, aligned_end_address;
	aligned_start_address = PAGE_ALIGN(curr_region->start_address);
	aligned_end_address = PAGE_ALIGN(curr_region->get_end_address());
	uint64_t page_len_bytes = (aligned_end_address - aligned_start_address);

	if (page_len_bytes == 0) {
		pr_err("WARN: Page length is zero\n");
		return LEN_IS_ZERO;
	}

	ret_moved_pages = -1;

	TierInfo *dst_tier = TINFO->getTierInfofromID(dst_virt_tier);
	if (curr_region->curr_virt_tier == dst_virt_tier)
		if (are_tiers_similar(curr_region->curr_virt_tier, dst_virt_tier)) {

			if (dst_tier->backing_store == FAST_NODE) {
				return ALREADY_IN_DRAM;
			}
			if (dst_tier->backing_store == SLOW_NODE) {
				return ALREADY_IN_OPTANE;
			}
			// if (dst_tier->mem_type == COMPRESSED) {
			//     /* its already in swap and in the correct tier. */
			//     return ALREADY_IN_ZSWAP;
			// }

			return ALREADY_SOMEWHERE;
		}

#ifdef ENABLE_NTIER
	/* move the page to a tier. If the dst tier was DRAM or Optane, it iwll bring the page out of zswap. */
	/* TODO
	This requires kernel code fixes.
	What this logic will do:
	1. wil fault on everything in the zswap and bring it back to DRAM.
	2. Then it checks, if the DST_TIER is 0 or more, it assumes that that is zswap tiers, and will move the data to those tiers.
	This second step can be taken out. That should be called from the user space.
	*/
	ret = syscall(SYS_do_migrate_dst_tier, pid, aligned_start_address, page_len_bytes, dst_virt_tier - COMPRESSED_TIERS_BASE);
	if (ret) {
		/* ENSURE YOU ARE ON THE RIGHT KERNEL */
		handle_syscall_error(curr_region, pid, dst_virt_tier);
		return ret; // either the applicaion is done.
	} else {
		ret_moved_pages = page_len_bytes / PAGE_SIZE;
	}

#endif // ENABLE_NTIER

	/* The page is guaranteed to be in DRAM  now. Now move it to the correct byte addressable tier.s */

	if (dst_tier->mem_type != COMPRESSED) {
		ret_moved_pages = move_to_dram_or_optane(curr_region, pid);
		if (ret_moved_pages < 0) {
			WARN_ONCE("move_to_dram_or_optane FAILED");
			ret = 1;
		}
	}
	if (!ret) {
		curr_region->curr_virt_tier = dst_virt_tier;
		curr_region->dst_virt_tier = -1;
	}

	return ret_moved_pages;
}

// hashmap
const unordered_map<int, string> error_map = {
    {12, "ENOMEM"},
    {-12, "ENOMEM"},
    {101, "EAGAIN"},
    {-101, "EAGAIN"},
    {102, "EEXIST"},
    {-102, "EEXIST"},
    {3, "ESRCH"},
    {-3, "ESRCH"}};

void handle_syscall_error(REGION_SKD *curr_region, int &pid, int &dst_virt_tier) {

	// define an array, if error number in that, dont print message
	int allowed_errors[] = {12, -12, 101, -101, 102, -102, 3, -3};
	int expected_error = 0;
	for (uint64_t i = 0; i < sizeof(allowed_errors) / sizeof(allowed_errors[0]); i++) {
		if (errno == allowed_errors[i]) {
			expected_error = 1;
			break;
		}
	}
	if (!expected_error) {
		pr_err("SYSCALL region_id %lu pid:%d tier:%d start_addr:%p end_addr:%p size_4k:%lu. hotness: %lu ERROR no %d\n", curr_region->region_id, pid, dst_virt_tier, (void *)curr_region->start_address, (void *)curr_region->get_end_address(), curr_region->size_4k, curr_region->get_hotness(),
		       errno);
	} else {
		pr_err("It was an expected error %d %s\n", errno, error_map.at(errno).c_str());
	}
}

const char *get_mem_type_string(MEM_TYPE type) {
	switch (type) {
case DRAM:
		return "DRAM";
case OPTANE:
		return "OPTANE";
case CXL:	
		return "CXL";
case HBM:
		return "HBM";
case COMPRESSED:
		return "COMPRESSED";
default:
		return "UNKNOWN";
	}
}
