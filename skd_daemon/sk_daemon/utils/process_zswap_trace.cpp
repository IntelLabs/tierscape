#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <sstream>


#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "all_mapping_header.h"
#include "all_region_header.h"
#include "migrate_thread.h"
#include "tier_utils.h"
#include "utils.h"

using namespace std;

extern atomic<bool> is_program_alive;
extern int total_compressed_tiers;
// extern TIERS_INFO *tiers_info;

#ifdef CONFIG_DISTRIBUT_FAULTS
extern REGION_BASE* region_logic;
#endif  // CONFIG_DISTRIBUT_FAULTS

static ifstream trace_file("/sys/kernel/debug/tracing/trace_pipe");

int toggle_tracing(int val) {
    ofstream enable_file("/sys/kernel/debug/tracing/events/kmem/zswap_zpool_stat/enable");

    if (!enable_file) {
        cerr << "Failed to open enable file\n";
        return 1;
    }

    fprintf(stderr, "Toggling zswap pool trace to %d\n", val);
    enable_file << val;

    enable_file.close();
    if (val == 0)
        trace_file.close();

    return 0;
}

void trace_intHandler(int dummy) {
    cout << "trace_intHandler: Caught USERSIGNAL " << dummy << endl;
    // exit(0);
    toggle_tracing(0);
    pthread_exit(nullptr);
    is_program_alive = false;
}



vector<TierInfo*> sysctl_data_entries;

extern char tier_stats_file[CHAR_FILE_LEN];
// uint64_t init_faults = 0, init_compressed_size = 0, init_nr_pages = 0;


#ifdef CONFIG_DISTRIBUT_FAULTS

void distribute_faults() {
    try {
        if (region_logic != nullptr && region_logic->regions.size() != 0) {
            // Distribute tier hotness to regions with same current tier
            for (uint64_t i = 0; i < sysctl_data_entries.size(); i++) {
                int virt_tier_id = sysctl_data_entries[i]->get_virt_tier_id();
                uint64_t faults = sysctl_data_entries[i]->faults;
                int num_regions = 0;
                if (faults > 10) {
                    for (size_t j = 0; j < region_logic->regions.size(); j++) {
                        if (region_logic->regions[j] && region_logic->regions[j]->curr_virt_tier == virt_tier_id) {
                            num_regions++;
                        }
                    }

                    if (num_regions > 0) {
                        /* only if the tier has some regions. */
                        uint64_t hotness_per_region = (faults / num_regions);
                        // if(hotness_per_region==0 && faults > 1000 && num_regions > 100){
                        //     hotness_per_region=200;
                        // }
                        if (hotness_per_region > 0) {
                            /* no point on doing this if we are not going to add anything. */
                            for (uint64_t j = 0; j < region_logic->regions.size(); j++) {
                                if (region_logic->regions[j] && region_logic->regions[j]->curr_virt_tier == virt_tier_id) {
                                    /* Do not update the actual hotness here. Let me updated at the end of the profile window.
                                    That should also reset it. */
                                    region_logic->regions[j]->hotness_from_faults += hotness_per_region;
                                }
                            }
                        }
                        else {
                            WARN_ONCE("WARN: Cannot distribute faults hotness_per_region\n");


                        }
                    }
                    else {
                        fprintf(stderr, "No regions for tier: %d with faults %ld region size: %ld\n", virt_tier_id, faults, region_logic->regions.size());
                    }
                }
            }
        }
        else {
            if (region_logic == nullptr) {
                pr_err("region_logic is NULL \n");
            }
            else if (region_logic->regions.size() == 0) {
                // pr_err("region_logic->regions.size() == 0\n");

            }
            else {
                WARN_ONCE("NOT SUPPORTED FOR region_logic is PEBS Single\n");
            }
        }
    }
    catch (...) {
        pr_err("Exception in distributing faults\n");
    }
}
#endif  // CONFIG_DISTRIBUT_FAULTS

void add_to_sysctl_data_entries(TierInfo* data, bool isFreshData) {
    // pr_debug("Trying to add to sysctl_data_entries\n");
    /* Fixing faults */
    uint64_t faults = data->faults;
    if (isFreshData) {
        data->last_seen_faults = faults;
    }

    if (faults >= data->last_seen_faults) {
        data->faults = (faults - data->last_seen_faults);
    }
    else {
        pr_err("Faults decreased. This should not happen. Faults: %lu, Last seen faults: %lu\n", faults, data->last_seen_faults);
    }

    data->last_seen_faults = faults;

    if (data->nr_pages != 0)
        data->compression_ratio = (float)(data->nr_compressed_size) / (float)(data->nr_pages * PAGE_SIZE);

    if (isFreshData)
        sysctl_data_entries.push_back(data);


    // pr_debug("Added to sysctl_data_entries: virt_tier_id: %lu, virt_tier_id: %d, nr_compressed_size: %lu, type: %s, compressor: %s, backing_store: %d, nr_pages: %lu, isCPU: %d, faults: %lu, compression_ratio: %f\n",
    //     data->virt_tier_id, data->virt_tier_id, data->nr_compressed_size, data->type.c_str(), data->compressor.c_str(), data->backing_store, data->nr_pages, data->isCPU, data->faults, data->compression_ratio);
}

TierInfo* get_sysctl_data_entry(uint16_t virt_tier_id) {
    // pr_debug("Trying to get sysctl_data_entry\n");
    for (auto data : sysctl_data_entries) {
        if (data->get_virt_tier_id() == virt_tier_id) {
            // pr_debug("Found entry for virt_tier_id: %lu\n", virt_tier_id);
            return data;
        }
        // else{
        //     pr_debug("No match virt_tier_id: %lu, data->virt_tier_id: %lu\n", virt_tier_id, data->virt_tier_id);
        // }
    }
    // pr_debug("No entry found for virt_tier_id: %lu\n", virt_tier_id);
    return nullptr;
}



struct ZswapStat {
    double timestamp = -1.0;
    int pool_id = -1;
    long nr_compressed_size = -1;
    std::string type;
    std::string compressor;
    int backing_store = -1;
    int nr_pages = -1;
    int isCPU = -1;
    int faults = -1;

    std::string toString() const {
        std::ostringstream oss;
        oss << "timestamp: " << timestamp
            << ", pool_id: " << pool_id
            << ", nr_compressed_size: " << nr_compressed_size
            << ", type: " << type
            << ", compressor: " << compressor
            << ", backing_store: " << backing_store
            << ", nr_pages: " << nr_pages
            << ", isCPU: " << isCPU
            << ", faults: " << faults;
        return oss.str();
    }
};

ZswapStat parseZswapStat(const std::string& input) {
    ZswapStat stat;

    // Extract timestamp using colon-delimited logic
    size_t ts_pos = input.find(':');
    if (ts_pos != std::string::npos) {
        // Scan backwards to find number just before colon
        size_t num_start = input.rfind(' ', ts_pos);
        if (num_start != std::string::npos && ts_pos > num_start + 1) {
            std::string ts_str = input.substr(num_start + 1, ts_pos - num_start - 1);
            try {
                stat.timestamp = std::stod(ts_str);
            } catch (...) {
                // fallback: leave timestamp as -1.0
            }
        }
    }

    // Now extract key=value stats
    size_t pos = input.find("zswap_zpool_stat");
    if (pos == std::string::npos) return stat;

    std::string statsPart = input.substr(pos + std::string("zswap_zpool_stat").length());
    std::istringstream iss(statsPart);
    std::string token;

    while (iss >> token) {
        size_t eqPos = token.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = token.substr(0, eqPos);
        std::string value = token.substr(eqPos + 1);

        if (key == "pool_id") stat.pool_id = std::stoi(value);
        else if (key == "nr_compressed_size") stat.nr_compressed_size = std::stol(value);
        else if (key == "type") stat.type = value;
        else if (key == "compressor") stat.compressor = value;
        else if (key == "backing_sotore" || key == "backing_store") stat.backing_store = std::stoi(value);
        else if (key == "nr_pages") stat.nr_pages = std::stoi(value);
        else if (key == "isCPU") stat.isCPU = std::stoi(value);
        else if (key == "faults") stat.faults = std::stoi(value);
    }

    return stat;
}


void* th_consume_zswap_pool_stats_events(void* ptr) {

    // if /proc/sys/kernel/zswap_print_stat does not exist, then we are not in zswap print error and return
    if (access("/proc/sys/kernel/zswap_print_stat", F_OK) == -1) {
        fprintf(stderr, "zswap_print_stat does not exist. Not in zswap mode\n");
        return nullptr;
    }
    


    signal(SIGUSR1, trace_intHandler);
    signal(SIGUSR2, trace_intHandler);

    

    struct data_packet* dp = (struct data_packet*)(ptr);

    int pid = dp->pid;
    // MAPPING_BASE* mapping_logic = dp->mapping_logic;
    if (!trace_file) {
        cerr << "Failed to open trace file: " << endl;
        return nullptr;
    }
    fprintf(stderr, "Starting zswap TRACE thread for PID %d\n", pid);

    toggle_tracing(1);
    string input_string;
    // int grp_sep = -1;
    // int curr_window = 0;

    // auto process_start_time = chrono::high_resolution_clock::now();

    FILE* log = fopen(tier_stats_file, "a+");



    while ((is_program_alive) && getline(trace_file, input_string)) {
        stringstream ss(input_string);
        vector<string> tokens;

        if (input_string.find("zswap_zpool_stat") == string::npos)
            continue; /* This is not a zswap trace. Some other trace is also enabled. */

        /* sysctl-58930   [001] ..... 57348.612691: zswap_zpool_stat: zswap_zpool_stat pool_id=3 nr_compressed_size=0 type=zbud compressor=lz4 backing_sotorem=0 nr_pages=0 isCPU=1 faults=0
        sysctl-58930   [001] ..... 57348.612694: zswap_zpool_stat: zswap_zpool_stat pool_id=4 nr_compressed_size=0 type=zsmalloc compressor=lzo-rle backing_sotorem=2 nr_pages=0 isCPU=1 faults=0
        sysctl-58930   [001] ..... 57348.612696: zswap_zpool_stat: zswap_zpool_stat pool_id=5 nr_compressed_size=0 type=zsmalloc compressor=lzo-rle backing_sotorem=0 nr_pages=0 isCPU=1 faults=0 */


        istringstream iss(input_string);
        ZswapStat stat = parseZswapStat(input_string);

        fprintf(log, "%s\n", stat.toString().c_str());


    }
    fclose(log);
    toggle_tracing(0);
    printf("Tier trace Thread done\n");
    return 0;

}

void* th_consume_zswap_pool_stats_events_2(void* ptr) {
    signal(SIGUSR1, trace_intHandler);
    signal(SIGUSR2, trace_intHandler);

    

    struct data_packet* dp = (struct data_packet*)(ptr);

    int pid = dp->pid;
    // MAPPING_BASE* mapping_logic = dp->mapping_logic;
    if (!trace_file) {
        cerr << "Failed to open trace file: " << endl;
        return nullptr;
    }
    fprintf(stderr, "Starting zswap TRACE thread for PID %d\n", pid);

    toggle_tracing(1);
    string input_string;
    // int grp_sep = -1;
    // int curr_window = 0;

    // auto process_start_time = chrono::high_resolution_clock::now();
    

    while ((is_program_alive) && getline(trace_file, input_string)) {
        stringstream ss(input_string);
        vector<string> tokens;

        if (input_string.find("zswap_zpool_stat") == string::npos)
            continue; /* This is not a zswap trace. Some other trace is also enabled. */

        /* sysctl-58930   [001] ..... 57348.612691: zswap_zpool_stat: zswap_zpool_stat pool_id=3 nr_compressed_size=0 type=zbud compressor=lz4 backing_sotorem=0 nr_pages=0 isCPU=1 faults=0
        sysctl-58930   [001] ..... 57348.612694: zswap_zpool_stat: zswap_zpool_stat pool_id=4 nr_compressed_size=0 type=zsmalloc compressor=lzo-rle backing_sotorem=2 nr_pages=0 isCPU=1 faults=0
        sysctl-58930   [001] ..... 57348.612696: zswap_zpool_stat: zswap_zpool_stat pool_id=5 nr_compressed_size=0 type=zsmalloc compressor=lzo-rle backing_sotorem=0 nr_pages=0 isCPU=1 faults=0 */


        istringstream iss(input_string);


        // cout << input_string << endl;

        // Skip the first 5 tokens in the string
        for (int i = 0; i <= 5; i++) {
            string token;
            iss >> token;
        }
        // pr_debug("Skipped first 5\n");

        // // Parse the fields and populate the struct
        string token;
        bool is_fresh_data = false;
        iss >> token;
        int virt_tier_id;
        try {
            virt_tier_id = stoi(token.substr(token.find("=") + 1));  // Extract the integer value of pool_id
        }
        catch (...) {
            pr_err("Error in parsing the trace file. Skipping this line\n");
            continue;
        }
        // pr_debug("saw virt_tier_id: %lu total_compressed_tiers: %d\n", virt_tier_id, total_compressed_tiers);

        if (virt_tier_id >= total_compressed_tiers) {
            continue; /* We are not bothered with other tiers. */
        }


        TierInfo* data = get_sysctl_data_entry(virt_tier_id);

        if (data == nullptr) {
            // pr_debug("Creating new entry for virt_tier_id: %lu\n", virt_tier_id);
            data = TINFO->getTierInfofromID(virt_tier_id + COMPRESSED_STARTS_FROM);
            if (data == NULL) {
                // This is the extra tier. Just skip it
                // pr_debug("Skipping extra tier %d as it is not in the tier list\n", virt_tier_id);
                continue;
            }
            data->faults = 0;
            data->nr_pages = 0;
            data->nr_compressed_size = 0;
            data->last_seen_faults = 0;
            is_fresh_data = true;
        }
        // lse{
        //     printf("Found entry for virt_tier_id: %lu\n", virt_tier_id);
        // }

        try {
            iss >> token;  // Get the second field ("nr_compressed_size")
            data->nr_compressed_size = stoul(token.substr(token.find("=") + 1));
            iss >> token;  // Get the third field ("type")
            data->type = COMPRESSED;
            // token.substr(token.find("=") + 1);
            iss >> token;  // Get the fourth field ("compressor")
            data->compressor = token.substr(token.find("=") + 1);
            iss >> token;  // Get the fifth field ("backing_store")
            data->backing_store = stoul(token.substr(token.find("=") + 1));
            iss >> token;  // Get the sixth field ("nr_pages")
            data->nr_pages = stoul(token.substr(token.find("=") + 1));
            iss >> token;  // Get the seventh field ("isCPU")
            data->isCPU = stoul(token.substr(token.find("=") + 1));
            iss >> token;  // Get the eighth and final field ("faults")
            /* This NEEDS to be fixed */
            data->faults = stoul(token.substr(token.find("=") + 1));
        }
        catch (...) {
            pr_err("Error in parsing the trace file. Skipping this line\n");
            continue;
        }

        // pr_debug("Adding to sysctl_data_entries\n");
        /* THis will fix the faults and will add to the list if new. */
        add_to_sysctl_data_entries(data, is_fresh_data);

        /* Logging the tiers to the file */
        if (data->get_virt_tier_id() == (total_compressed_tiers - 1+COMPRESSED_STARTS_FROM)) { /* We have seen all the tiers in this stats window */
            FILE* log = fopen(tier_stats_file, "a+");
            // pr_debug("Opened file: %s\n", tier_stats_file);
            // pr_debug("Logging the tiers to the file as virt_tier_id is %d and we need %d\n", data->virt_tier_id, total_compressed_tiers - 1);
            // pr_debug("To file: %s\n", tier_stats_file);
            /* last entries */
            uint64_t total_compressed_data_dram, total_compressed_data_optane;
            uint64_t total_pages_dram, total_pages_optane;
            uint64_t total_fautls_dram, total_fautls_optane;
            int next_id = 0;
            total_compressed_data_optane = 0;
            total_compressed_data_dram = 0;
            total_pages_dram = 0;
            total_pages_optane = 0;
            total_fautls_dram = 0;
            total_fautls_optane = 0;



            for (TierInfo* sd : sysctl_data_entries) {
                // Print the parsed data
                if (sd->get_virt_tier_id() != next_id) {
                    pr_debug("Skipping %d as next_id is %d\n", sd->virt_tier_id, next_id);
                    continue;
                }
                else
                    next_id++;

                // pr_debug("Saving %d next_id is %d\n", sd->virt_tier_id, next_id);

                fprintf(log, "tier_id: %d (%f), nr_c_size: %lu, type: %s, comp: %s, BS: %d, nr_pages: %lu, isCPU: %d, faults: %lu\n",
                    sd->get_virt_tier_id(), sd->compression_ratio,
                    sd->nr_compressed_size, get_mem_type_string(sd->type), sd->compressor.c_str(), sd->backing_store, sd->nr_pages, sd->isCPU, sd->faults);

                if (sd->backing_store == 0) {
                    total_compressed_data_dram += sd->nr_compressed_size;
                    total_pages_dram += sd->nr_pages;
                    total_fautls_dram += sd->faults;
                }
                else if (sd->backing_store == 2) {
                    total_compressed_data_optane += sd->nr_compressed_size;
                    total_pages_optane += sd->nr_pages;
                    total_fautls_optane += sd->faults;
                }
            }

            fprintf(log,
                "total_compressed_data_dram: %lu\n"
                "total_compressed_data_optane: %lu\n"
                "total_pages_dram: %lu\n"
                "total_pages_optane: %lu\n"
                "total_fautls_dram: %lu\n"
                "total_fautls_optane: %lu\n",
                total_compressed_data_dram, total_compressed_data_optane, total_pages_dram, total_pages_optane, total_fautls_dram, total_fautls_optane);

            fclose(log);
#ifdef CONFIG_DISTRIBUT_FAULTS
            // pr_debug("Distributing faults\n");
            distribute_faults();

#endif
        }else{
            cout << input_string << endl;
            pr_info("Not logging the tiers to the file as virt_tier_id is %d and we need %d\n", data->get_virt_tier_id(), total_compressed_tiers - 1);
        }
    }

    toggle_tracing(0);
    printf("Tier trace Thread done\n");
    return 0;
}