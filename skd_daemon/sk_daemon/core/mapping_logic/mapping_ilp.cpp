
#include "mapping_ilp.h"

#include <iterator>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>

#include "model.h"
#include "comms_model.h"    
extern float SKD_HOT_TH;
using namespace std;

#define PORT 8080

extern std::atomic<bool> is_program_alive;

extern vector<AddrEntry*> child_addr_vector;
extern int c_count;

int sock = 0, valread;

int get_env_int_or_default(const std::string& var_name, int default_value = 0) {
    const char* val = std::getenv(var_name.c_str());
    if (!val) return default_value;

    try {
        return std::stoi(val);
    } catch (...) {
        return default_value;
    }
}

int connect_to_server() {
    struct sockaddr_in serv_addr;
    // std::vector<obj*> objects;

    // Create client socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cout << "Socket creation error" << std::endl;
        return -5;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    char * ILP_SERVER = (char*)malloc(100);

    int remote_server = get_env_int_or_default("REMOTE_MODE", 0);
    if (remote_server==1) {
        pr_info("Connecting to remote ILP server\n");
        //  ILP_SERVER = REMOTE_ILP_SERVER;
         sprintf(ILP_SERVER, "%s", REMOTE_ILP_SERVER);
    } else {
        pr_info("Connecting to local ILP server\n");
        // ILP_SERVER = LOCAL_ILP_SERVER;
        sprintf(ILP_SERVER, "%s", LOCAL_ILP_SERVER);
    }
    


    // Convert IP address from string to binary form
    if (inet_pton(AF_INET, ILP_SERVER, &serv_addr.sin_addr) <= 0) {
        pr_err("Invalid address/ Address not supported %s\n", ILP_SERVER);
        return -5;
    }

    // Connect to the server
    int retries = 3000;
    while (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        if (errno == ECONNREFUSED && retries-- > 0) {
            usleep(1000);
            continue;
        }
        pr_err("Waited for %d seconds. Connection refused to %s on port %d. Errorno: %d %s \n", 3, ILP_SERVER, PORT, errno, strerror(errno));
        
        return -2;
    }

    return 0;
}

vector<uint64_t> hotness_vals;
vector<TierInfo_Comm*> tiers_comm;

void transform_to_comm(vector<REGION_SKD*> regions) {
    vector<TierInfo*> tiers = TIERS_INFO::get_instance()->tiers;


    tiers_comm.clear();
    hotness_vals.clear();


    for (size_t i = 0; i < tiers.size(); i++) {
        TierInfo_Comm* tier = new TierInfo_Comm();
        

        tier->virt_tier_id = tiers[i]->get_virt_tier_id();
        tier->virt_tier_id = TINFO->get_idx_from_virtid(tier->virt_tier_id);
        tier->isCPU = tiers[i]->isCPU;
        tier->backing_store = tiers[i]->backing_store;
        tier->compression_ratio = tiers[i]->compression_ratio;

        tier->tier_cost = tiers[i]->tier_cost;
        tier->tier_latency = tiers[i]->tier_latency;

        // printf tier information
        pr_debug("Tier %ld: virt_tier_id %d isCPU %d backing_store %d compression_ratio %.2f tier_cost %d tier_latency %d\n", i, tier->virt_tier_id, tier->isCPU, tier->backing_store, tier->compression_ratio, tier->tier_cost, tier->tier_latency);



        tiers_comm.push_back(tier);
    }

    tiers.erase(tiers.begin());

    for (size_t i = 0; i < regions.size(); i++) {
        hotness_vals.push_back((regions[i])->get_hotness());
    }
}

// static int window = 0;
// void simplify_move_pages(vector<REGION_SKD*> regions) {
//     window++;
//     if (window == 1) {
//         // everything will be in DRAM. No exchange opportunit
//         pr_debug("First window. No exchange opportunity\n");
//         // return;
//     }
//     int total_recommended_migratons = 0;
//     vector<REGION_SKD*> regions_copy;
//     for (size_t i = 0; i < regions.size(); i++) {
//         REGION_SKD* curr_region = regions[i];
//         if (curr_region->curr_virt_tier != curr_region->dst_virt_tier && curr_region->curr_virt_tier != VIRT_DRAM_TIER_ID) {
//             total_recommended_migratons++;
//             regions_copy.push_back(curr_region);
//         }
//     }

//     pr_debug("Total recommended migrations: %d\n", total_recommended_migratons);
//     pr_debug("Total candidates %ld\n", regions_copy.size());
//     int total_matches = 0;

//     for (size_t i = 0; i < regions_copy.size(); i++) {
//         // for each region find a region with same hotness whose curr_virt_tier is same as this ones dest_virt_tier, and then remove both from the list
//         REGION_SKD* curr_region = regions_copy[i];
//         for (size_t j = i; j < regions_copy.size(); j++) {
//             if (i == j)
//                 continue;
//             REGION_SKD* other_region = regions_copy[j];
//             //             30 have different hotness 0 0 or different dest tier -22(-4) 3
//             // -------INFO: Region 3 and 8631 have different hotness 0 0 or different dest tier -22(-4) 3
//             // -------INFO: Region 3 and 8632 have different hotness 0 0 or different dest tier -22(-4) 3
//             // -------INFO: Region 3 and 8633 have different hotness 0 0 or different dest tier -22(-4) 3
//             // -------INFO: Region 3 and 8634 have different hotness 0 0 or different dest tier -22(-4) 3
//             // -------INFO: Region 3 and 8635 have different hotness 0 0 or different dest tier -22(-4) 3
//             // -------INFO: Region 3 and 8636 have different hotness 0 0 or different dest tier -22(-4) 3
//             // -------INFO: Region 3 and 8637 have different hotness 0 0 or different dest tier -22(-4) 3
//             // -------INFO: Region 3 and 8638 have different hotness 0 0 or different dest tier -22(-4) 3
//             // -------INFO: Region 3 and 8639 have different hotness 0 0 or different dest tier -22(-4) 3

//             if ((curr_region->get_hotness() == other_region->get_hotness()) && (curr_region->curr_virt_tier == other_region->dst_virt_tier)) {
//                 // pr_debug("Region %d and %d have same hotness and same dest tier\n", i, j);
//                 regions_copy.erase(regions_copy.begin() + j);
//                 regions_copy.erase(regions_copy.begin() + i);
//                 i--;

//                 total_matches++;
//                 break;
//             }
//         }
//     }

//     pr_debug("Total regions after simplification %ld total matches %d regions removed %d candidates left %d\n", regions_copy.size(), total_matches, total_matches * 2, (total_recommended_migratons - (total_matches * 2)));

//     // for (int i = 0; i < regions.size(); i++) {
//     //     regions_copy.push_back(regions[i]);
//     // }

// }

int MAPPING_ILP::start_ilp_server(){
    pr_info("Starting ILP server\n");
    char command[300];
    sprintf(command, "${SKD_HOME_DIR}/start_fresh_ilp_server.sh %.2f\n", SKD_HOT_TH);
    
    int result = std::system(command);
    if (result == 0) {
        std::cout << "Command executed successfully." << std::endl;
    }
    else {
        std::cerr << "FATAL Command execution failed." << std::endl;
    }
    return result;
}

int MAPPING_ILP::process_regions(vector<REGION_SKD*>* regions) {
    pr_debug("Processing regions using ILP\n");

    // Construct the command with arguments
    
    int ret = 0;
    // bool second_chance=false;
// trytoconnectagain:

    if (connect_to_server() != 0 ) {
        pr_err("Failed to connect to server. Trying to start a new instance\n");
        // char command[300];

        // if (second_chance) {
        //     pr_info("Second chance. Not starting a new instance\n");
        //     return -1;
        // }
        // sprintf(command, "${SKD_HOME_DIR}/start_fresh_ilp_server.sh %.2f\n", SKD_HOT_TH);


        // pr_debug("Executing %s\n", command);
        // int result = std::system(command);
        // if (result == 0) {
        //     std::cout << "Command executed successfully." << std::endl;
        //     second_chance = true;
        //     goto trytoconnectagain;
        // }
        // else {
        //     std::cerr << "FATAL Command execution failed." << std::endl;
            return -1;
        // }
    }
    else {
        pr_info("Connected to server\n");
    }
    TIERS_INFO* t_info = TIERS_INFO::get_instance();
    vector<TierInfo*> tiers = t_info->tiers;

    /* NOTE: The order of sending must be maintained */
    /* Send the number of tiers and regions to be expected.
    We send tiers agian as the compressibility may change.
    TODO going further, the average latency may also change. */

    int nr_tiers = tiers.size();
    int nr_regions = regions->size();

    if (nr_tiers == 0 || nr_regions == 0) {
        pr_debug("No tiers or regions to send\n");
        throw("No tiers or regions to send. This should not happen.");
        return -1;
    }

    transform_to_comm(*regions);
    nr_tiers = tiers_comm.size(); 


    send(sock, &nr_tiers, sizeof(nr_tiers), 0);
    send(sock, &nr_regions, sizeof(nr_regions), 0);


    /* Send the tiers first */
    for (int i = 0; i < nr_tiers; i++) {
        send(sock, tiers_comm[i], sizeof(TierInfo_Comm), 0);
    }
    // fprintf(stderr, "Sent tiers\n");


    /* Send regions next */
    for (int i = 0; i < nr_regions; i++) {
        send(sock, &(hotness_vals[i]), sizeof(uint64_t), 0);
    }
    pr_debug("Tiers info sent to the ILP server.. Waiting for response\n");
    auto ilp_start_time = std::chrono::high_resolution_clock::now();  // get start time





    try {
        // Receive updated objects from the server
        // unordered_map<int, int> countMap;

        DataPacket* dp = new DataPacket();
        valread = read(sock, dp, sizeof(DataPacket));

        auto ilp_end_time = std::chrono::high_resolution_clock::now();                                    // get current time
        auto ilp_elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(ilp_end_time - ilp_start_time);  // calculate elapse
        pr_info("Time taken by the ILP server %ld ms\n", ilp_elapsed_time.count());

        dp->toString();
        if (dp->failed == false) {
            for (int i = 0; i < nr_regions; i++) {
                /* ================ Main Crux of the Operation ======================================= */
                valread = read(sock, &((*regions)[i]->dst_virt_tier), sizeof(int));
                REGION_SKD* curr_region = (*regions)[i];
                if (curr_region->dst_virt_tier < 0 || curr_region->dst_virt_tier >= nr_tiers) {
                    pr_err("INFO Invalid tier %d for region %d\n", curr_region->dst_virt_tier, i);
                    throw("Invalid tier from ILP server");
                }
                curr_region->dst_virt_tier = TINFO->get_virt_id_from_idx(curr_region->dst_virt_tier);
                
            }

            ret =0;
        }
        else {
            // fake read 0
            int tmp = 0;
            for (int i = 0; i < nr_regions; i++) {
                valread = read(sock, &tmp, sizeof(int));
            }
            ret = -1;
        }

        // /* ---------------------------- Just to print stalls belw ---------------------------------------------------------- */
        // /* ensure count map has all the tiers. Missing one will be marked as 0. Rest will be untouched.*/
        // /* just to ensure all the i's are present in countMap.  */
        // for (int i = 0; i < nr_tiers; i++) {
        //     countMap[i]++;
        //     countMap[i]--;
        // }
        // pr_debug("Recommendations from ILP Received %d tiers and %d regions\n", nr_tiers, nr_regions);
        // std::map<int, int> sortedMap(countMap.begin(), countMap.end());
        // for (const auto& pair : sortedMap) {
        //     // std::cout << "Tier: " << pair.first << ", Regions: " << pair.second << std::endl;
        //     pr_debug("Tier: %d, Regions: %d\n", pair.first, pair.second);
        // }


    }
    catch (const std::exception& e) {
        std::cout << "Caught exception: " << e.what() << std::endl;
    }


    // for (int i = 0; i < min(10, nr_regions); i++) {
    //     fprintf(stderr, "Region %d: %d\n", i, (*regions)[i]->dst_virt_tier);
    // }

    try {
        close(sock);
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }

    /* too costly */
    // simplify_move_pages(*regions);

    return ret;
}

