
#include <numa.h>
#include <numaif.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

#include "all_mapping_header.h"
#include "all_region_header.h"
#include "tier_utils.h"
#include "utils.h"
#include "migrate_thread.h"

using namespace std;

extern std::atomic<bool> is_program_alive;
extern vector<AddrEntry*> child_addr_vector;
extern int c_count;

// extern TIERS_INFO *tiers_info;
extern REGION_BASE* region_logic;
extern MAPPING_BASE* mapping_logic;
extern SKD_MODE skd_mode;
extern int total_tiers, total_compressed_tiers;

extern vector<float> percentile_val_arr;

extern char migrate_log_file[CHAR_FILE_LEN];




void** addrs = nullptr;
int* status = nullptr;
int* nodes = nullptr;

void migrate_compress_regions_parallel(REGION_BASE* region_logic, int pid, int window_seconds, int dram_optane_mode);

// static int last_idx_processed = 0;
atomic<unsigned long> page_counter(0);
atomic<unsigned long> region_counter(0);
atomic<unsigned long> already_in_dst(0);

atomic<unsigned long> already_in_dram(0);
atomic<unsigned long> already_in_optane(0);
atomic<unsigned long> already_in_zswap(0);
atomic<unsigned long> already_in_somewhere(0);

atomic<unsigned long> migration_candidates(0);

int nr_push_window = 0;



extern uint64_t global_start_addr, global_end_addr;
double percentage = 0;
auto migrate_start_time = std::chrono::high_resolution_clock::now();  // get start time
// ===========================================



extern int push_threads;
int thread_count;                 // number of threads
vector<uint64_t> thread_last_reach_ctr;
atomic<unsigned long> threds_ctr(0);
int stop_threads = 0;
int _window_seconds = 0;

int is_there_time() {

    auto current_time = std::chrono::high_resolution_clock::now();                                    // get current time
    auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - migrate_start_time);  // calculate elapsed time

    int seconds_left = _window_seconds - elapsed_time.count();
    if (seconds_left <= 0) {
        pr_warn("MIGRATE Time limit reached. Exiting. Time taken %ld seconds\n", elapsed_time.count());
        seconds_left = 0;
        stop_threads = 2;
    }

    return seconds_left;


}


std::mutex print_lock;

void thread_helper_pusher(int _thread_id, uint64_t actual_start_idx, uint64_t end_index, int pid, int dram_optane_mode) {

    int thread_id = _thread_id;
    uint64_t mod_start_idx = actual_start_idx;
    uint64_t j;
    bool BYTE_MODE = (bool)dram_optane_mode;
    bool ZSWAP_MODE = (bool)(!dram_optane_mode);

    if (stop_threads || !is_there_time()) {
        return;
    }


    if (ZSWAP_MODE) {
        /* zswap mode. We only save the index and resume for zswaps. Nothing else. */
        if (thread_last_reach_ctr[thread_id] == 0)
            thread_last_reach_ctr[thread_id] = actual_start_idx;
        mod_start_idx = thread_last_reach_ctr[thread_id];


        if (mod_start_idx >= end_index) {
            pr_debug("RESETTING %d. mod_start_idx: %lu end_index: %lu\n", thread_id, mod_start_idx, end_index);
            mod_start_idx = actual_start_idx;
            thread_last_reach_ctr[thread_id] = actual_start_idx;
        }
    }

    pr_debug("Starting Thread %u: mod_start_idx: %lu end_index: %lu\n", thread_id, mod_start_idx, end_index);

    for (j = mod_start_idx; j < end_index; j++) {
        REGION_SKD* curr_region = region_logic->regions[j];
        int dst_virt_tier = curr_region->dst_virt_tier;

        if (dst_virt_tier < COMPRESSED_STARTS_FROM && ZSWAP_MODE) {
            continue;
        }
        else if (dst_virt_tier >= COMPRESSED_STARTS_FROM && BYTE_MODE) {
            continue;
        }
        migration_candidates.fetch_add(1);

        /* ********************** */
        /* ******** MAGIC *********** */
        int moved_pages = push_a_region(curr_region, pid, dst_virt_tier);

        /* ***************************** */
        if (moved_pages > 0) {
            /* something was pushed */
            page_counter.fetch_add((unsigned long)moved_pages);
            region_counter.fetch_add(1);
            if (page_counter.load() >= MAXPAGES)
            {
                std::scoped_lock lock(print_lock);
                // pr_warn("PAGES LIMIT HIT. Exiting. %ld\n", page_counter.load());
                stop_threads = 1;
            }
        }
        else {

            switch (moved_pages) {
            case ALREADY_IN_DRAM:
                already_in_dram.fetch_add(1);
                break;
            case ALREADY_IN_OPTANE:
                already_in_optane.fetch_add(1);
                break;
            case ALREADY_IN_ZSWAP:
                already_in_zswap.fetch_add(1);
                break;
            case ALREADY_SOMEWHERE:
                already_in_somewhere.fetch_add(1);
                break;
            case 0:
                pr_warn("PAGES NOT MOVED. %d\n", moved_pages);
                break;
            default:
                // WARN_ONCE("Unknown return value from push_a_region \n");
                pr_warn("Unknown return value from push_a_region %d\n", moved_pages);
                exit(1);
            }
        }

        if (stop_threads || !is_there_time()) {
            break;
        }

    }

    /* if (!dram_optane_mode) {
        thread_last_reach_ctr[thread_id] = j;

        pr_debug("ZSWAP_MODE\n");
        pr_debug("TH[%d]: Quanta (%d) is_transfer_limit_hit:%d regions pushed: %ld (of %ld) start_idx %ld end_idx %ld. offset %ld \n", \
            thread_id, _window_seconds, stop_threads, (j - mod_start_idx), (end_index - mod_start_idx), mod_start_idx, end_index, thread_last_reach_ctr[thread_id]);

    } */


    // take print lock
    if (thread_id == 0)
    {
        /* released automatically */
        std::scoped_lock lock(print_lock);

        uint64_t val_candidates = migration_candidates.load();
        uint64_t val_region_counter = region_counter.load();
        uint64_t val_already_in_dram = already_in_dram.load();
        uint64_t val_already_in_optane = already_in_optane.load();
        uint64_t val_already_in_zswap = already_in_zswap.load();
        uint64_t val_already_in_somewhere = already_in_somewhere.load();


        pr_info("TH[%d]: DP_Md:%d CD:%ld Regions:%ld", thread_id, dram_optane_mode, val_candidates, val_region_counter);
        printf("| AL: DRAM:%lu OPTANE:%lu ZSWAP:%lu SOME:%lu. STOP[OK,MaxPg,TimLim]:%d. VAL(0?): %ld\n", val_already_in_dram, val_already_in_optane, val_already_in_zswap, val_already_in_somewhere, stop_threads, (val_candidates - val_region_counter - val_already_in_dram - val_already_in_optane - val_already_in_zswap - val_already_in_somewhere));
        // flush print
        fflush(stdout);
    }




    return;
}

void migrate_compress_regions_parallel(REGION_BASE* region_logic, int pid, int window_seconds, int dram_optane_mode = 0) {
    uint64_t nr_p = region_logic->regions.size();


    thread_count = push_threads;
    uint64_t chunk_size = nr_p / thread_count;  // numb er of iterations per thread
    stop_threads = 0;
    _window_seconds = window_seconds;

    pr_debug("***********PUSHING PAGES. REGIONS: %ld using threads %d DRAM_OPTANE_MODE %d (seconds %d)***********\n", nr_p, push_threads, dram_optane_mode, is_there_time());

    /* setting the reach counters. Done only once. */
    if (thread_last_reach_ctr.size() == 0)
        for (int i = 0;i < thread_count;i++)
            thread_last_reach_ctr.push_back(0);


    std::vector<std::thread> threads;                             // vector to store the threads.


    // create threads
    for (int i = 0; i < thread_count; i++) {
        uint64_t actual_start_idx = (i * chunk_size);
        uint64_t end_index = (i + 1) * chunk_size;

        if (i == (thread_count - 1)) {
            end_index = nr_p;  // The last thread handles the rest of the iterations
        }

        threads.push_back(std::thread(thread_helper_pusher, i, actual_start_idx, end_index, pid, dram_optane_mode));

    }

    for (auto& thread : threads) {
        thread.join();
    }


    pr_debug("***********DONE PUSHING PAGES. REGIONS: %ld using threads %d DRAM_OPTANE_MODE %d***********\n", nr_p, push_threads, dram_optane_mode);


}


extern int OPTANE_PREFERRED;
int generate_low_event_warn = 2;

void* th_process_events_and_migrate(int pid, int window_seconds) {

    if (child_addr_vector.size() == 0) {
        pr_err("Region logic is null. Exiting. ");
        return NULL;
    }

    if (!is_program_alive) {
        pr_debug("Process died.");
        return NULL;
    }

    if (region_logic == nullptr) {
        pr_err("Region logic is null. Exiting. ");
        return NULL;
    }
    /* It must be initialized before. */
    if (region_logic->is_init() == 0) {
        region_logic->init_initial_regions(pid);
        region_logic->set_is_init(1);
    }

    int dram_optane_mode = -1;


    /* ===================================================== */
    pr_info("-------- New Profile Window -----------\n")
        pr_info("PEBS PROCESSING: Total regions: %ld Total events captured: %ld \n", region_logic->regions.size(), child_addr_vector.size());

    if (child_addr_vector.size() < 100) {
        generate_low_event_warn--;
    }
    if (generate_low_event_warn == 0) {
        WARN_ONCE("INCREASE THE PEBS FREQ");
    }

    if (child_addr_vector.size() == 0) {
        pr_err("No events to process. Exiting.\n");
        return NULL;
    }

    region_logic->fix_regions();

    pr_info("Address space is %p to %p Size:%luMB. Total regions: %ld\n", (void*)global_start_addr, (void*)global_end_addr, (global_end_addr - global_start_addr) / (1024 * 1024), region_logic->regions.size());

    /* ===================================================== */

    auto start_time = std::chrono::high_resolution_clock::now();  // get start time
    int ret = region_logic->assign_events_to_regions(child_addr_vector);

    if (ret != SKD_SUCCESS) {
        pr_err("There was an error while processing the events. ERROR: %d\n", ret);
        return NULL;
    }

    if (mapping_logic == nullptr) {
        pr_err("Mapping logic is null. Exiting. ");
        return NULL;
    }



    ret = mapping_logic->process_regions(&region_logic->regions);
    if (ret == -1) {
        pr_err("Mapping logic failed. Exiting.\n");
        exit(1);

    }

    /* print unsorted version */
    mapping_logic->print_regions_info(region_logic->regions, 0);

    // /* THis is truye for NONE */
    // if (mapping_logic->disable_migration == true) {
    //     pr_debug("Migration is disabled. Exiting.\n");
    //     return NULL;
    // }



    // // sort regions based or dst_virt_id
    // pr_info("Sorting to prefer promotions to DRAM.\n");
    // sort(region_logic->regions.begin(), region_logic->regions.end(),
    //     [](const REGION_SKD* a, const REGION_SKD* b) {
    //         return a->dst_virt_tier < b->dst_virt_tier;
    //     });



    if (ret == -1) {
        pr_err("process_regions FAILED. Initiating migrate threads. For left overs\n");

    }
    else {
        unordered_map<int, int> countMap;
        int nr_tiers = TIERS_INFO::get_instance()->get_nr_tiers();

        for (auto& region : region_logic->regions) {
            countMap[region->dst_virt_tier]++;
        }
        for (int i = 0; i < nr_tiers; i++) {
            countMap[TINFO->get_virt_id_from_idx(i)]++;
            countMap[TINFO->get_virt_id_from_idx(i)]--;
        }
        {
            std::scoped_lock lock(print_lock);
            pr_info("Recommendations from Mapping Received %d tiers and %ld regions\n", nr_tiers, region_logic->regions.size());
            std::map<int, int> sortedMap(countMap.begin(), countMap.end());
            for (const auto& pair : sortedMap) {
                printf("[%d]%d ", pair.first, pair.second);
            }
            printf("\n");
            pr_debug("Proceeding with Migration thread\n");
            fflush(stdout);
        }
    }


    auto end_time = std::chrono::high_resolution_clock::now();                                    // get current time
    auto pebs_elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    pr_info("PEBS PROCESSING: Time taken to process_regions (not migration): %ld ms\n", pebs_elapsed_time.count());

    pr_debug("Push window is %d regions pushed %lu pages pushed is %lu limit is %u \n", nr_push_window, region_counter.load(), page_counter.load(), MAXPAGES);
    if (open_pagemap_file(pid)) {
        pr_err("Failed to open PAGEMAP file for pid %d", pid);
    }


    migrate_start_time = std::chrono::high_resolution_clock::now();  // get start time

    dram_optane_mode = OPTANE_PREFERRED;
    reset_counters();
    migrate_compress_regions_parallel(region_logic, pid, window_seconds, dram_optane_mode);


    dram_optane_mode = (1 - dram_optane_mode);
    reset_counters();
    migrate_compress_regions_parallel(region_logic, pid, window_seconds, dram_optane_mode);



    if (close_pagemap_file()) {
        pr_err("Failed to close PAGEMAP file for pid %d", pid);
    }

    auto current_time = std::chrono::high_resolution_clock::now();                                    // get current time
    auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - migrate_start_time);  // calculate elapse
    pr_debug("PEBS PROCESSING: Time taken to actually push the pages: %ld seconds\n", elapsed_time.count());




    // TODO fix this
    unordered_map<int, int> countMap;
    int nr_tiers = TIERS_INFO::get_instance()->get_nr_tiers();

    for (auto& region : region_logic->regions) {
        countMap[region->curr_virt_tier]++;
    }
    for (int i = 0; i < nr_tiers; i++) {
        countMap[TINFO->get_virt_id_from_idx(i)]++;
        countMap[TINFO->get_virt_id_from_idx(i)]--;
    }
    {
        std::scoped_lock lock(print_lock);
        pr_info("State after migration %d tiers and %ld regions\n", nr_tiers, region_logic->regions.size());
        std::map<int, int> sortedMap(countMap.begin(), countMap.end());
        for (const auto& pair : sortedMap) {
            printf("[%d]%d ", pair.first, pair.second);
        }
        printf("\n");
        fflush(stdout);
    }

    pr_debug(" ========> Push window was %d \n-------\n", nr_push_window);

    nr_push_window++;


    return NULL;
}
void reset_counters()
{
    page_counter.store(0);
    region_counter.store(0);

    already_in_dst.store(0);

    already_in_dram.store(0);
    already_in_optane.store(0);
    already_in_zswap.store(0);
    already_in_somewhere.store(0);

    migration_candidates.store(0);
}