// #include "mapping_google.h"

// using namespace std;

// extern std::atomic<bool> is_program_alive;

// extern vector<AddrEntry *> child_addr_vector;
// extern int c_count;

// int MAPPING_GOOGLE::process_regions(vector<REGION_SKD *> *regions) {
//     struct REGION_SKD *curr_region;
//     vector<REGION_SKD *> hot_regions;
//     // uint64_t aligned_start_address, aligned_end_address;
//     // int target_tier = 0;
//     // int is_only_unaccessed_pages = 0;

//     if (!is_program_alive) {
//         pr_debug("Process died.");
//         return 1;
//     }

//     /* Calculate the dst tier based on the adjusted hotness */
//     for (uint64_t i = 0; i < (*regions).size(); i++) {
//         curr_region = (*regions)[i];
//         if(curr_region->get_hotness() > 0){
//             curr_region->dst_virt_tier = TINFO->get_fast_tier_virt_id();
//         }
//         else {
//             curr_region->dst_virt_tier = TINFO->get_slow_tier_virt_id();
//         }
        
//     }

//     return 0;
// }

// extern char regions_log_file[CHAR_FILE_LEN];
// void MAPPING_GOOGLE::print_regions_info(vector<REGION_SKD*> regions, int hotness_threshold) {

//     (void)hotness_threshold;
//     (void)regions;

//     if(disable_print){
//         pr_debug("MAPPING_GOOGLE print_regions_info disabled\n");
//         return;
//     }
// }
