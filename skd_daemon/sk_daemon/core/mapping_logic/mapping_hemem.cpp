#include "mapping_hemem.h"

using namespace std;


int MAPPING_HEMEM::process_regions(vector<REGION_SKD*>* regions) {
    struct REGION_SKD* curr_region;
    vector<REGION_SKD*> hot_regions;
    // char cmd[400];
    // int ret;
    // uint64_t aligned_start_address, aligned_end_address;
    // int target_tier = 0;
    // int is_only_unaccessed_pages = 0;

    if (!is_program_alive) {
        pr_debug("Process died.");
        return 1;
    }

    if (SKD_HOT_TH < 1) {
        pr_err("FATAL Invalid SKD_HOT_TH value %f\n", SKD_HOT_TH);
        return -1;
    }

    uint64_t hot_threshold;
    bool is_found = false;
    // find idx of SKD_HOT_TH in percentile_arr
    for (size_t i = 0; i < percentile_arr.size(); i++) {
        if (percentile_arr[i] == SKD_HOT_TH) {
            hot_threshold = (int)percentile_val_arr[i];
            pr_info("-- Mapping HEMEM. Using a percentile %f hot_threshold of %ld SLOW tier %d\n", SKD_HOT_TH, hot_threshold, TINFO->get_slow_tier_virt_id())
            is_found = true;
            break;
        }
    }

    // if it was not in the percentile mode.
    if (!is_found) {
        hot_threshold = (int)SKD_HOT_TH;
        pr_info("Mapping HEMEM. Using a direct hot_threshold of %ld SLOW tier %d\n", hot_threshold, TINFO->get_slow_tier_virt_id());
    }
    

    /* Calculate the dst tier based on the adjusted hotness */
    for (uint64_t i = 0; i < (*regions).size(); i++) {
        curr_region = (*regions)[i];
        if (curr_region->get_hotness() >= hot_threshold)
            curr_region->dst_virt_tier = TINFO->get_fast_tier_virt_id();
        else
            curr_region->dst_virt_tier = TINFO->get_slow_tier_virt_id();
    }

    return 0;
}


