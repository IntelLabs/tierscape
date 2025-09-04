
#include "mapping_waterfall.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>

#include "model.h"
#include "comms_model.h"    
using namespace std;

extern int c_count;
extern int total_tiers;
extern pid_t pid;

#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/mman.h>

char filename[BUFSIZ];

extern vector<float> percentile_val_arr, percentile_arr;


uint64_t calculate_number_of_4k_pages_in_swap(unsigned long start_address, unsigned long end_address, int fd) {
    uint64_t total_pages_in_swap = 0;

    if (fd < 0) {
        pr_err("FATAL: Error opening file %s\n", filename);
        exit(1);
    }

    // pr_debug("Calculating number of 4k pages in swap for region %lx - %lx\n", start_address, end_address);
    // pr_debug("Calculating number of 4k pages in swap for region %lx - %lx\n", PAGE_ALIGN(start_address), PAGE_ALIGN(end_address));

    // start_address = PAGE_ALIGN(start_address);
    // end_address = PAGE_ALIGN(end_address);

    for (uint64_t i = start_address; i < end_address; i += 0x10000) {
        uint64_t data;
        uint64_t index = (i / PAGE_SIZE) * sizeof(data);
        if (pread(fd, &data, sizeof(data), index) != sizeof(data)) {
            perror("pread");
            pr_err("FATAL: Error reading file %s fd %d\n", filename, fd);
            exit(1);
        }
        // print_page(i, data);
        if (((data >> 62) & 1) == 1ul) {
            total_pages_in_swap++;
        }
        // ctr++;
        // if (ctr == 10) {
        //     break;
        // }
    }

    return total_pages_in_swap;
}

// define a map of <int, int> if size total_tiers
// key is the tier number and value is the number of regions in that tier
map<int, int> tier_region_count;


int MAPPING_WATERFALL::process_regions(vector<REGION_SKD*>* regions) {
    pr_debug("Processing regions using WATERFALL. Region Size %ld\n", regions->size());
    auto start_time = std::chrono::high_resolution_clock::now();  // get start time

    int ret = 0;
    int nr_regions = regions->size();
    // int ctr_swapped = 10;

    // reset tier_region_count to all zero
    for (int i = 0;i < total_tiers;i++) {
        tier_region_count[i] = 0;
    }

    sprintf(filename, "/proc/%d/pagemap", pid);
    pr_debug("Opening %s\n", filename);
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("open");
        pr_err("FATAL: Error opening file %s\n", filename);
        exit(1);
    }
    pr_debug("Opened %s %d\n", filename, fd);

    uint64_t nr_hot_update = 0,  nr_swap_dram_upgrade = 0;


    uint64_t hot_threshold;
    pr_debug("SKD_HOT_TH %f\n", SKD_HOT_TH);
    // find idx of SKD_HOT_TH in percentile_arr
    bool found = false;
    for (size_t i = 0; i < percentile_arr.size(); i++) {
        if (percentile_arr[i] == SKD_HOT_TH) {
            pr_debug("Found SKD_HOT_TH %f at index %ld val_arr is %f\n", SKD_HOT_TH, i, percentile_val_arr[i]);
            hot_threshold = percentile_val_arr[i];
            pr_debug("Mapping Waterfall. Using a percentile %f hot_threshold of %ld \n", SKD_HOT_TH, hot_threshold);
            found = true;
            break;
        }

    }

    if (!found) {
        hot_threshold = (int)SKD_HOT_TH;
        pr_debug("Mapping Waterfall. Using a direct hot_threshold of %ld \n", hot_threshold);
    }


    for (int i = 0; i < nr_regions; i++) {

        if (i % ((nr_regions) / 10) == 0) {
            pr_debug("------- Processing region %d of %d\n", i, nr_regions);
        }

        REGION_SKD* region = regions->at(i);

        int curr_waterfall_tier = TINFO->get_fast_tier_virt_id();

        /* very HOT. Should be in DRAM */
        if (region->get_hotness() >= hot_threshold) {
            region->dst_virt_tier = curr_waterfall_tier;
            // pr_debug("UPGRADE region %d:  hotness %llu\n", i, region->get_hotness());
            nr_hot_update++;
            tier_region_count[curr_waterfall_tier]++;
            continue;
        }

        /* COLD REGIONS */


        if (region->dst_virt_tier < 0) {
            /* processed */
            curr_waterfall_tier = TINFO->get_next_tier_virt_id(region->curr_virt_tier);
            region->dst_virt_tier = curr_waterfall_tier;
            tier_region_count[curr_waterfall_tier]++;
            continue;
        }
        else {
            /* not processed */
            curr_waterfall_tier = TINFO->get_next_tier_virt_id(region->dst_virt_tier);
            region->dst_virt_tier = curr_waterfall_tier;
            tier_region_count[curr_waterfall_tier]++;
        }

        // /* Even if its not pushed in the swap, its offical tier starts getting demoted, so that when the push thread reaches this, it is pushed in its correct tier. */
        // if (region->dst_virt_tier != (total_tiers - 1)) {
        //     region->dst_virt_tier = region->dst_virt_tier + 1;
        //     tier_region_count[region->dst_virt_tier]++;
        // }

        // /* invalid or DRAM */
        // if (region->curr_virt_tier <= PHY_OPTANE_TIER_ID) {
        //     /* not yet pushed by the migration thread or in DRAM. NO point in checking the number of pages in the swap, as everythig will be in the swap */
        //     continue;
        // }


        /* this is shortcircuting a region.. if a number of pages are NOT in swap */
        unsigned long long pages_in_swap = calculate_number_of_4k_pages_in_swap(region->start_address, region->get_end_address(), fd);
        if ((pages_in_swap == 0) && region->dst_virt_tier >= 2) {
            curr_waterfall_tier = TINFO->get_fast_tier_virt_id();
            region->dst_virt_tier = curr_waterfall_tier;
            nr_swap_dram_upgrade++;
            tier_region_count[curr_waterfall_tier]++;

        }
    }
    pr_debug("NEW: Hot upgrades %lu Swap DRAM upgrades %lu\n", nr_hot_update,  nr_swap_dram_upgrade);
    // print tier_region_count
    for (int i = 0;i < total_tiers;i++) {
        pr_debug("Tier %d: %d\n", i, tier_region_count[i]);
    }
    close(fd);


    auto  current_time = std::chrono::high_resolution_clock::now();                                    // get current time
    auto  elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);  // calculate elapse
    pr_debug("***MAPPING PROCESSING: Time taken to process_regions: %ld seconds\n", elapsed_time.count());



    return ret;

}
