#include "mapping_scatter.h"

using namespace std;

extern std::atomic<bool> is_program_alive;

extern vector<AddrEntry*> child_addr_vector;
extern int c_count;

float tier_limits[] = {
    10  // -1 DRAM
    ,
    15  // 0 25
    ,
    15  // 1 40
    ,
    15  // 2 55
    ,
    10  // 3 65
    ,
    10  // 4 75
    ,
    10  // 5 85
    ,
    10  // 6 95
    ,
    5  // 7 100
};
int getTier(float input) {
    float start_val = 100;
    int ret = 7;
    for (int i = 0; i < 9; i++) {
        start_val -= tier_limits[i];
        if (input >= start_val) {
            ret = (i - 1);
            break;
        }
    }
    ret = min(7, ret);   // cannot be more then 7
    ret = max(-1, ret);  // cannot be less than -1
    return ret;
}


int MAPPING_SCATTER::process_regions(vector<REGION_SKD*>* regions) {
    pr_debug("Processing regions\n");

      /* Calculate the percentile */
    // populate_percentile_hotness(*regions);
    throw "Replace with convolved percentile";

    for (size_t i = 0; i < regions->size(); ++i) {
        REGION_SKD* curr_region = (*regions)[i];
        (*regions)[i]->dst_virt_tier = getTier(curr_region->percentile_hotness);
    }

    return 0;
}

extern char regions_log_file[CHAR_FILE_LEN];

