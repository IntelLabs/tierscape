#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <mutex>
#include <vector>

#include "all_mapping_header.h"
#include "all_region_header.h"
#include "migrate_thread.h"
#include "model.h"
#include "utils/utils.h"

std::mutex myMutex;

using namespace std;

pid_t pid;
int  push_threads;
int total_tiers, total_compressed_tiers;
uint64_t window_seconds = 0;
float SKD_HOT_TH = 25.0f;
vector<float> percentile_val_arr, percentile_arr;

SKD_MODE skd_mode;

struct region* regions = nullptr;
pthread_t t_tid, c_tid, z_tid, p_tid;

static vector<AddrEntry*> addr_vector;
vector<AddrEntry*> child_addr_vector;
std::atomic<bool> is_program_alive(true);

int c_count = 0;
char pebs_log_file[CHAR_FILE_LEN] = "";
char regions_log_file[CHAR_FILE_LEN] = "";
char gauss_log_file[CHAR_FILE_LEN] = "";
char tier_stats_file[CHAR_FILE_LEN] = "";
char migrate_log_file[CHAR_FILE_LEN] = "";

vector<REGION_SKD*> all_regions;
REGION_BASE* region_logic = nullptr;
MAPPING_BASE* mapping_logic = nullptr;

int slow_tier=-1;

uint64_t global_start_addr = 0l, global_end_addr = 0l;
atomic<unsigned long> perf_event_discard_count(0);

void* th_pebs_window_timeout(void* v_pid) {
    // char cmd[CHAR_FILE_LEN];
    int* pid = (int*)v_pid;
    // pid_t c_pid, wpid;
    // int status = 0;

    if (region_logic == nullptr || mapping_logic == nullptr) {
        pr_err("region_logic or mapping_logic is null\n");
        exit(1);
    }

    uint64_t time_taken_seconds = 0;
    while (is_program_alive) {
        if (time_taken_seconds < window_seconds) {
            pr_debug("Sleeping for %ld seconds\n", window_seconds - (int)time_taken_seconds);
            sleep(window_seconds - (int)time_taken_seconds);
        }
        else {
            pr_warn("Window time exceeded: time taken %lu allowed %ld\n", time_taken_seconds, window_seconds);
        }

        pr_debug("TIMEOUT th_pebs_window_timeout: %d\n", c_count);

        time_taken_seconds = 0;
        c_count++;
        struct timeval fstop, fstart;
        gettimeofday(&fstart, NULL);
        // ============= MAIN LOGIC========================================================
        if (addr_vector.size() > 0) {
            /* This is the core login that is executed every window_seconds. */

            myMutex.lock();
            auto start_time = std::chrono::high_resolution_clock::now();  // get start time
            copy(addr_vector.begin(), addr_vector.end(), back_inserter(child_addr_vector));
            addr_vector.clear();
            myMutex.unlock();
            auto current_time = std::chrono::high_resolution_clock::now();                                    // get current time
            auto elapsed_time = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time);  // calculate elapse
            pr_debug("ADDR COPY PROCESSING: Time taken to copy events: %ld seconds\n", elapsed_time.count());

            /* THis is truye for NONE */
            if (mapping_logic->disable_migration == true) {
                pr_info("Migration is disabled. Total events: %ld\n", child_addr_vector.size());
            }
            else {
                th_process_events_and_migrate(*pid, window_seconds * .9);

            }

            child_addr_vector.clear();
        }
        else {
            pr_warn("\n=====No events to process. %ld discarded events in last window %u\n=\n", perf_event_discard_count.load(), c_count);
            perf_event_discard_count = 0;
        }
        // ============= MAIN LOGIC========================================================
        gettimeofday(&fstop, NULL);
        time_taken_seconds = (fstop.tv_sec - fstart.tv_sec) * 1000000 + fstop.tv_usec - fstart.tv_usec;
        time_taken_seconds = time_taken_seconds / (1000000);
        pr_debug("Time in seconds %lu by window %d\n", time_taken_seconds, c_count);
    }
    // while ((*pid = wait(&status)) > 0);
    pr_debug("periodic thread: DONE\n");
    return NULL;
}

/* KEY: Consuming perf events. */

void* th_consume_perf_events(void* pid) {
    string lineInput;
    (void)pid;
    while (getline(cin, lineInput) && is_program_alive) {
        try {
            // fprintf(stderr, "th_consume_perf_events: %s\n", lineInput.c_str());
            int pos = lineInput.find(":");
            float addr_time = stof(lineInput.substr(0, pos - 1));
            uint64_t process_addr = stoull(lineInput.substr(15), nullptr, 16);

            // if (global_start_addr != 0) {
            //     if (process_addr < global_start_addr || process_addr > global_end_addr) {
            //         perf_event_discard_count++;
            //         continue;
            //     }
            // }
            process_addr = PAGE_ALIGN(process_addr);

            myMutex.lock();
            addr_vector.push_back(new AddrEntry(addr_time, process_addr));
            myMutex.unlock();

        }
        catch (const char* e) {
            pr_err("FATAL: Error while process perf event. %s\n", lineInput.c_str());
            exit(1);
        }
    }

    return NULL;
}

void* th_consume_damon_events(void* ptr);
int toggle_tracing(int val);

void intHandler(int code) {
    if (code == SIGINT)
        printf("Caught Ctrl+C\n");

    if (code == SIGUSR2)
        printf("Caught SIGUSR2\n");


    is_program_alive = false;
    toggle_tracing(0);
    // wait for 5 seconds and then exit the whole application
    sleep(5);
    exit(0);
}

FILE* opt_file_out = NULL;  ///< standard outpu
// TIERS_INFO *tiers_info;

void* th_consume_zswap_pool_stats_events(void* ptr);
int count_compressed_pools();
string get_perf_mode_string(int perf_mode_int);

int main(int argc, char** argv) {

    pid = -1;

    push_threads = DEFAULT_PUSH_THREADS;
    window_seconds = DEFAULT_WINDOW_SECONDS;
    skd_mode = HEMEM_MODE;
    // SKD_HOT_TH = DEFAULT_SKD_THRESHOLD;

    total_tiers = TINFO->get_nr_tiers();
    total_compressed_tiers = count_compressed_pools();

    bool disable_migration = false;
    


    // pr_info_highlight("Total tiers: %d total_compressed_tiers: %d\n", total_tiers, total_compressed_tiers);
    // total_tiers = total_compressed_tiers + 2;

    opt_file_out = stdout;
    int c;

    try {
        while ((c = getopt(argc, argv, ":h:p:f:w:e:o:l:t:n:s:c:d:")) != -1) {
            switch (c) {
                // case '-':
                //     fprintf(stderr, "Reading -\n");
                //     break;
            case 'p':
                fprintf(stderr, "Reading pid\n");
                pid = atoi(optarg);
                break;
            case 'w':
                fprintf(stderr, "Reading w\n");
                window_seconds = atoi(optarg);
                break;
            case 'o':
                fprintf(stderr, "Reading o\n");
                skd_mode = (SKD_MODE)atoi(optarg);
                break;
            case 'l':
                fprintf(stderr, "Reading l\n");
                strcpy(pebs_log_file, optarg);
                break;
            case 't':
                fprintf(stderr, "Reading t\n");
                throw "t Not implemented";
                break;
            case 's':
                fprintf(stderr, "Reading s\n");
                strcpy(tier_stats_file, optarg);
                break;
            case 'c':
                fprintf(stderr, "Reading c\n");
                SKD_HOT_TH = atof(optarg);
                break;
            case 'e':
                fprintf(stderr, "Reading e \'%s\'\n", optarg);
                push_threads = atoi(optarg);
                break;
            case 'd':
                fprintf(stderr, "Reading d \'%s\'\n", optarg);
                disable_migration = atoi(optarg);
                break;
            case 'n':
                // fprintf(stderr, "Reading n\n");
                // // if (total_tiers != atoi(optarg)) {
                //     // fprintf(stderr, "WARNING: Detected pools %d mismatch with passed pools %d\n", total_tiers, atoi(optarg));
                // // }
                break;
            case 'h':
                fprintf(stderr, "Reading h\n");
                slow_tier = atoi(optarg);
                break;
            default:
                pr_warn("Invalid option: %c in TRACKER\n", c);

            }
        }
    }
    catch (const char* e) {
        pr_err("Error: %s\n", e);
        return -1;
    }
    if (pid == -1) {
        pr_err("Need a PID. use -p\n");
        return -1;
    }

    fprintf(stderr,        "Initializing SKD with\n PID: %d\n push_threads: %d\n window_seconds: %ld\n  skd_mode %d\n SKD_HOT_TH %.2f\n SLOW TIER %d\n Tier stats file %s\n total_tiers %d",
            pid, push_threads, window_seconds, skd_mode, SKD_HOT_TH, slow_tier, tier_stats_file,total_tiers);

    fprintf(stderr, "perf_input_mode_str %s\n", get_perf_mode_string(skd_mode).c_str());

    // ================



    strcpy(regions_log_file, pebs_log_file);
    char* loc = strstr(regions_log_file, "pebs");
    if (loc != NULL) {
        strcpy(loc, "regions");
    }
    pr_debug("regions_log_file: %s\n", regions_log_file);

    strcpy(migrate_log_file, pebs_log_file);
    loc = strstr(migrate_log_file, "pebs");
    if (loc != NULL) {
        strcpy(loc, "migrate");
    }
    pr_debug("migrate_log_file: %s\n", migrate_log_file);


    strcpy(gauss_log_file, pebs_log_file);
    loc = strstr(gauss_log_file, "pebs");
    if (loc != NULL) {
        strcpy(loc, "gauss");
    }
    pr_debug("gauss_log_file: %s\n", gauss_log_file);

    // print log files name
    pr_debug("pebs_log_file: %s\n", pebs_log_file);
    pr_debug("tier_stats_file: %s\n", tier_stats_file);

    // ==========================================================
    // see sk_daemon/convolve.py
    percentile_arr.push_back(25);
    percentile_arr.push_back(50);
    percentile_arr.push_back(75);
    percentile_arr.push_back(90);
    percentile_arr.push_back(95);
    percentile_arr.push_back(99);


    if (!is_process_running(pid)) {
        pr_err("No such process %d\n", pid);
        return -1;
    }

    signal(SIGINT, intHandler);
    signal(SIGUSR2, intHandler);



    address_range ar = get_address_range(pid);
    global_start_addr = ar.start_addr;
    global_end_addr = ar.end_addr;

    switch (skd_mode) {
    case HEMEM_MODE:

        region_logic = new REGION_PEBS_FIXED();
        mapping_logic = new MAPPING_HEMEM();




        break;

    case NTIER_FIXED_ILP:
        region_logic = new REGION_PEBS_FIXED();
        mapping_logic = new MAPPING_ILP();


        break;

    case NTIER_FIXED_WATERFALL:
        region_logic = new REGION_PEBS_FIXED();
        mapping_logic = new MAPPING_WATERFALL();

        

        break;
      
    default:
        pr_err("Invalid perf input mode\n");
        exit(1);
        break;
    }

    if(disable_migration== true) {
        pr_info("Disabling migration as requested by the user.\n");
        mapping_logic->disable_migration = true;
    }


    pthread_create(&c_tid, NULL, th_consume_perf_events, &pid);
    pthread_create(&t_tid, NULL, th_pebs_window_timeout, &pid);

#ifdef ENABLE_NTIER
   pthread_create(&z_tid, NULL, th_consume_zswap_pool_stats_events, &pid);
#endif

    while (is_process_running(pid) && is_program_alive) {
        sleep(1);
    }

    is_program_alive = false;

    if (c_tid) {
        pr_debug("Process Died. Waiting for consume thread to return\n");
        pthread_kill(c_tid, SIGUSR1);
        // pthread_join(c_tid, &ret);
    }
    if (t_tid != 0) {
        pr_debug("Process Died. Waiting for process thread to return\n");
        pthread_kill(t_tid, SIGKILL);
        // pthread_join(t_tid, &ret);
    }

    pr_debug("PARENT: Total events remained: %ld\n", addr_vector.size());
    addr_vector.clear();

    return 0;
}
