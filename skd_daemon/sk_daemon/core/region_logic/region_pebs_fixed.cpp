// #include <filesystem>
#include "region_pebs_fixed.h"

#include <iostream>
#include <string>

#include "addr_space_util.h"
#include "utils.h"

using namespace std;

extern std::atomic<bool> is_program_alive;
extern SKD_MODE skd_mode;
extern vector<AddrEntry*> child_addr_vector;
extern int c_count;
// extern char pebs_log_file[CHAR_FILE_LEN];
extern char gauss_log_file[CHAR_FILE_LEN];
extern vector<float> percentile_val_arr;
extern uint64_t global_start_addr, global_end_addr;

// uint64_t daddr_start, daddr_end;
uint64_t total_regions;
//  = 4000;
const uint64_t REGION_SIZE = 2097152;  // in B is equal to 2MB

int REGION_PEBS_FIXED::fill_regions() {
    /* clear and free regions if its len is not 0 */
    vector<REGION_SKD*> temp_regions;
    if (regions.size() != 0) {
        // copy it to a new list
        for (size_t i = 0; i < regions.size(); i++) {
            // if hotness is more than 0
            if (regions[i]->get_hotness() > 0) {
                temp_regions.push_back(regions[i]);
            }
        }

        /* The regions should be empty. Else they are alread initated */
        pr_debug("fill_regions: Regions are not empty. Clearing them\n");
        free_vector(regions);
        regions.clear();


    }

    uint64_t len_addr_space = global_end_addr - global_start_addr;
    // total_regions = len_addr_space/(10*1024*1024*1024ul);

    pr_info("Init Address space is %p to %p %luMB. Total regions: %ld\n", (void*)global_start_addr, (void*)global_end_addr, len_addr_space / (1024 * 1024), total_regions);

    // uint64_t REGION_SIZE = 20971520; // 20MB
    // len_addr_space / total_regions;
    total_regions = len_addr_space / REGION_SIZE;  // regions of size 2MB.
    uint64_t idx = 0;
    uint64_t region_start_addr = global_start_addr + idx * REGION_SIZE;
    uint64_t region_end_addr = region_start_addr + REGION_SIZE;

    // for (uint64_t idx = 0; idx <= total_regions; idx++)
    while (region_end_addr < (global_end_addr + REGION_SIZE)) {
        /* start address, end adress,  */
        REGION_SKD* new_reg = new REGION_SKD(region_start_addr, region_end_addr);
        new_reg->region_id = idx;

        regions.push_back(new_reg);

        idx++;
        region_start_addr = global_start_addr + idx * REGION_SIZE;
        region_end_addr = region_start_addr + REGION_SIZE;
    }
    total_regions = regions.size();

    // iterate throguh temp_regions and copy the hotness to the new regions if the start and end address falls in between
    for (size_t i = 0; i < temp_regions.size(); i++) {
        for (size_t j = 0; j < regions.size(); j++) {
            if (temp_regions[i]->start_address >= regions[j]->start_address && temp_regions[i]->get_end_address() <= regions[j]->get_end_address()) {
                regions[j]->set_hotness(temp_regions[i]->get_hotness());
                regions[j]->collected_hotness = temp_regions[i]->collected_hotness;
                #ifdef CONFIG_DISTRIBUT_FAULTS
                    regions[j]->hotness_from_faults = temp_regions[i]->hotness_from_faults;
                #endif
                // regions[j]->time = temp_regions[i]->time;
                break;
            }
        }
    }


    return 0;
}


/* Here we divide the address space in equal regions */
int REGION_PEBS_FIXED::init_initial_regions(int pid) {
    if (is_init())
        return 0;

    if (regions.size() != 0) {
        /* The regions should be empty. Else they are alread initated */
        return 0;
    }
    pr_debug("\n************\nINITIALIZE REGIONS. THIS SHOULD BE DONE ONLY ONCE.\n************\n")

        o_pid = pid;
    address_range ar = get_address_range(pid);
    uint64_t addr_space_len = ar.end_addr - ar.start_addr;
    pr_info("Size in mb: %lu start in hex %lx end in hex %lx\n", addr_space_len / (1024 * 1024), ar.start_addr, ar.end_addr);

    uint64_t  new_start = ar.start_addr;
    uint64_t new_end = ar.end_addr;

    uint64_t size_in_mb = (new_end - new_start) / (1024 * 1024);

    if ((global_start_addr == new_start) && (global_end_addr == new_end)) {
        pr_debug("Init regions Address space is same.\n");
    }
    else if (size_in_mb == 0) {
        pr_err("Init Address space len is 0 MB.\n");
    }
    else {
        pr_debug("Init regions Address space changed. New size in %luMB\n", size_in_mb);
        global_start_addr = ar.start_addr;
        global_end_addr = ar.end_addr;
    }


    fill_regions();

    set_is_init(1);
    pr_debug("Init done. Total regions %ld of size %luMB\n", regions.size(), REGION_SIZE / (1024 * 1024));

    return 0;
}



int REGION_PEBS_FIXED::fix_regions() {

    // if (regions.size() <= 0) {
    //     return 0;
    // }

    address_range ar = get_address_range(o_pid);
    uint64_t  new_start = ar.start_addr;
    uint64_t new_end = ar.end_addr;

    if ((global_start_addr == new_start) && (global_end_addr == new_end)) {
        // pr_debug("Address space is same. No need to fix regions\n");
        /* nothing to be dine */
        pr_debug("Address space is same. No need to fix regions\n");
        // uint64_t len_addr_space = global_end_addr - global_start_addr;
        // pr_info("Address space is %p to %p %luMB. Total regions: %ld ", (void*)global_start_addr, (void*)global_end_addr, len_addr_space / (1024 * 1024), total_regions);
        return 0;
    }
    else {
        pr_info("New address detected in fix_regions\n");
    }


    /* if they have changed ensure that new size is more than the last one */
    if ((new_end - new_start) <= (global_end_addr - global_start_addr)) {
        pr_warn("Address space has changed but its shrinked. New size in %luMB\n", (new_end - new_start) / (1024 * 1024));
        /* checif the new region is inside old one */
        if (new_start >= global_start_addr && new_end <= global_end_addr) {
            pr_warn("New address space is inside the old one. \n");
        }
        else {
            pr_warn("New address space is not inside the old one. \n");
        }
        return 0;
    }

    else {
        pr_warn("ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR ERROR Address space changed. Need to fix regions.\n New size in %luMB\n", (new_end - new_start) / (1024 * 1024));
    }

    global_start_addr = new_start;
    global_end_addr = new_end;
    uint64_t len_addr_space = global_end_addr - global_start_addr;
    pr_info("Fixed Address space is %p to %p %luMB. Total regions: %ld\n", (void*)global_start_addr, (void*)global_end_addr, len_addr_space / (1024 * 1024), total_regions);

    fill_regions();


    return 2;

}

// bool compare_region_by_time(const REGION_SKD* a, const REGION_SKD* b) {
//     return a->time < b->time;
// }

bool is_sorted(const std::vector<REGION_SKD*>& regions) {
    return std::is_sorted(regions.begin(), regions.end(), [](const REGION_SKD* a, const REGION_SKD* b) {
        return a->start_address < b->start_address;
        });
}

bool is_sorted(const std::vector<AddrCountEntry*>& regions) {
    return std::is_sorted(regions.begin(), regions.end(), [](const AddrCountEntry* a, const AddrCountEntry* b) {
        return a->page_addr < b->page_addr;
        });
}

void print_hotness_bar(vector<REGION_SKD *> regions) {
    std::vector<uint64_t> flat_hotness;
    for (const auto& r : regions) {
            flat_hotness.push_back(r->get_hotness());
    }

    size_t total = flat_hotness.size();
    const int bar_width = 100;
    size_t block_size = std::max<size_t>(1, total / bar_width);

    // ==============

    // 🔥 Lambda for mapping hotness sum to a character
    auto map_hotness = [](uint64_t sum) -> char {
        if (sum == 0) return '.';
        if (sum < 100) return ':';
        if (sum < 1000) return '*';
        if (sum < 10000) return '+';
        if (sum < 50000) return 'W';
        return 'H';
    };

    uint64_t max_sum = max_element(flat_hotness.begin(), flat_hotness.end()) - flat_hotness.begin();
    auto map_hotness_min_max = [max_sum](uint64_t sum) -> char {
        double ratio = static_cast<double>(sum) / max_sum;
        if (ratio == 0.0) return '.';
        if (ratio < 0.02) return ':';
        if (ratio < 0.1) return '*';
        if (ratio < 0.3) return '+';
        if (ratio < 0.7) return 'W';
        return 'H';
    };

    auto map_hotness_log = [](uint64_t sum) -> char {
        if (sum == 0) return '.';
        int level = static_cast<int>(log10(sum));
        switch (level) {
            case 0: return ':';
            case 1: return '*';
            case 2: return '+';
            case 3: return 'W';
            default: return 'H';
        }
    };
    
    // ==============

    std::string bar, bar_min_max, bar_log;
    for (size_t i = 0; i < total; i += block_size) {
        uint64_t sum = 0;
        for (size_t j = i; j < std::min(i + block_size, total); ++j)
            sum += flat_hotness[j];
        bar += map_hotness(sum);
        bar_min_max += map_hotness_min_max(sum);
        bar_log += map_hotness_log(sum);
    }
    // print legend
    std::cout << "Legend: . : * + W H\n";
    std::cout <<"Hard: " << bar << std::endl;
    std::cout << "MinM: " << bar_min_max << std::endl;
    std::cout << "Log: "<< bar_log << std::endl;
}


vector<AddrCountEntry*> scup;
int REGION_PEBS_FIXED::assign_events_to_regions(vector<AddrEntry*> page_addrs) {
    
    scup.clear();
    uint64_t nr_assigned_addrs = 0, nr_unassigned_addrs = 0, nr_no_matching_regions = 0;
    sort_count_unique_process_mod(page_addrs, global_start_addr, global_end_addr, &scup);
    // n = scup.size();
    if (scup.size() == 0) {
        pr_err(" Warning: List is empty\n");
        return -1;
    }

    pr_debug("PEBS PROCESSING: SCUP size: %ld \n", scup.size());


    if (scup.size() == 1) {
        pr_warn("Only one entry in SCUP. This is not good. \n");
        
        return 0;
    }

    // bool assigned = 0;

    sort(regions.begin(), regions.end(),
        [](const REGION_SKD* a, const REGION_SKD* b) {
            return (a->start_address < b->start_address);
        });
    sort(scup.begin(), scup.end(),
        [](const AddrCountEntry* a, const AddrCountEntry* b) {
            return (a->page_addr < b->page_addr);
        });


    uint64_t size_in_mb = (scup[scup.size() - 1]->page_addr - scup[0]->page_addr) / (1024 * 1024);
    pr_debug("SCUP covered area: %p to %p. In MB it is %lu \n", (void*)scup[0]->page_addr, (void*)scup[scup.size() - 1]->page_addr, size_in_mb);

    if (size_in_mb > 500000) {
        /* 500 GB and this is also a lot */
        pr_err("FATAL ERROR. ALlowed size is 500GB. But this is %lu GB \n", size_in_mb / 1000);
        pr_err("FATAL ERROR. ALlowed size is 500GB. But this is %lu GB \n", size_in_mb / 1000);
        pr_err("FATAL ERROR. ALlowed size is 500GB. But this is %lu GB \n", size_in_mb / 1000);
        pr_err("FATAL ERROR. ALlowed size is 500GB. But this is %lu GB \n", size_in_mb / 1000);
        pr_err("FATAL ERROR. ALlowed size is 500GB. But this is %lu GB \n", size_in_mb / 1000);
        pr_err("FATAL ERROR. ALlowed size is 500GB. But this is %lu GB \n", size_in_mb / 1000);

        exit(1);
    }

    // for each region in regions reset collected_hotness to 0
    for (uint64_t i = 0; i < total_regions; i++) {
        regions[i]->collected_hotness = 0;
    }


    for (uint64_t i = 0; i < scup.size(); i++) {
        if (scup[i]->page_addr < global_start_addr || scup[i]->page_addr > global_end_addr) {
            nr_unassigned_addrs++;
            continue;
        }

        uint64_t zero_shifted = scup[i]->page_addr - global_start_addr;
        int y = zero_shifted / REGION_SIZE;




        if (covers_the_val(scup[i]->page_addr, regions[y])) {
            regions[y]->collected_hotness += scup[i]->count;
            // regions[y]->time = scup[i]->addr_time;
            nr_assigned_addrs++;


            // break;
        }
        else {
            nr_no_matching_regions++;
        }


    }

    // ==============================================================
    // ==============================================================

    // save hotness as array to a file /tmp/hotness
    FILE* hotness_file = fopen("/tmp/hotness", "w");
    for (uint64_t y = 0; y < total_regions; y++) {
        fprintf(hotness_file, "%lu\n", regions[y]->collected_hotness);
    }
    fclose(hotness_file);

    // executed a python script in the same folder named concolve.py
    char cmd[CHAR_FILE_LEN+100];
    sprintf(cmd, "python3 ${SKD_HOME_DIR}/sk_daemon/convolve.py %s\n", gauss_log_file);
    system(cmd);

    // read the output of the python script from /tmp/hoteness_update and update the hotness of all the regions
    FILE* hotness_update_file = fopen("/tmp/hotness_updated", "r");
    for (uint64_t y = 0; y < total_regions; y++) {
        fscanf(hotness_update_file, "%lu", &regions[y]->collected_hotness);
    }
    fclose(hotness_update_file);
    pr_debug("Convolved updated\n");

    // ==============================================================
    // ==============================================================


    // if file /tmp/comm exits read the first line from file and split based on space, create a float array from it
    percentile_val_arr.clear();
    if (FILE* file = fopen("/tmp/comm", "r")) {
        char line[100];
        fgets(line, sizeof(line), file);
        fclose(file);
        char* pch;
        pch = strtok(line, " ");
        while (pch != NULL) {
            percentile_val_arr.push_back(atof(pch));
            pch = strtok(NULL, " ");
        }
    }



    for (uint64_t y = 0; y < total_regions; y++) {
#ifdef CONFIG_DISTRIBUT_FAULTS
        regions[y]->set_hotness(regions[y]->collected_hotness + regions[y]->hotness_from_faults * .8);
        regions[y]->hotness_from_faults = 0;
        regions[y]->collected_hotness = 0;
#else
        regions[y]->set_hotness(regions[y]->collected_hotness);
        regions[y]->collected_hotness = 0;
#endif
    }

    // ========

    print_hotness_bar(regions);
    // std::vector<uint64_t> all_hotness;
    // for (const auto& r : regions) {
    //     all_hotness.push_back(r->get_hotness());
    // }

    
    // auto get_bucket_start = [](uint64_t hotness) -> uint64_t {
    //     if (hotness == 0) return 0;
    //     else if (hotness <= 100) return 1;
    //     else if (hotness <= 1000) return 101;
    //     else if (hotness <= 10000) return 1001;
    //     else return 10001;
    // };

    // auto format_bucket = [](uint64_t bucket_start) -> std::string {
    //     if (bucket_start == 0) return "0";
    //     else if (bucket_start == 1) return "1-100";
    //     else if (bucket_start == 101) return "101-1000";
    //     else if (bucket_start == 1001) return "1001-10000";
    //     else return "10001+";
    // };
    
    

    // std::string output;
    // uint64_t count = 0;
    // uint64_t prev_bucket = UINT64_MAX;

    // for (size_t i = 0; i < all_hotness.size(); ++i) {
    //     uint64_t bucket = get_bucket_start(all_hotness[i]);

    //     if (i == 0 || bucket == prev_bucket) {
    //         count++;
    //     } else {
    //         output += "[" + format_bucket(prev_bucket) + ":" + std::to_string(count) + "]";
    //         count = 1;
    //     }

    //     prev_bucket = bucket;
    // }

    // if (count > 0) {
    //     output += "[" + format_bucket(prev_bucket) + ":" + std::to_string(count) + "]";
    // }

    // std::cout << output << std::endl;


    // =====



    if (nr_no_matching_regions > 0 || nr_unassigned_addrs > 0) {
        pr_err("\n WARN: PEBS: Assigned %lu addresses to %lu regions. Unassigned: %lu No matching regions: %lu\n\n", nr_assigned_addrs, total_regions, nr_unassigned_addrs, nr_no_matching_regions);
    }


    scup.clear();

    return 0;
}
