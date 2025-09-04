
// #include <stdio.h>
// #include <stdlib.h>
// #include <sys/mman.h>
// #include <sys/stat.h>
// #include <sys/time.h>
// #include <sys/types.h>
// #include <sys/wait.h>
// #include <time.h>
// #include <unistd.h>

// #include <fstream>
// #include <iostream>
// #include <vector>


// #include "utils.h"
// #include "region_pebs_smart.h"
// #include "addr_space_util.h"

// using namespace std;




// int REGION_PEBS_SMART::init_initial_regions(int _pid) {
    
//     return 0;
// }


// /* Given a set of addresses the the profiler has seen, return a set of regions that encompasses those addresses.
// Note that it only processes the addresses present int the list.
//  */
// static void get_initial_accessed_regions(vector<AddrEntry *> page_addrs,  vector<REGION_SKD *> regions) {
   
//     return;
// }

// /* The function add region with access count as 0 */
// /* Takes input a set of regions and a range of address.
// It validates whether the set of regions covers all of the address space or not. If some of the addresses are not covered then it adds those addresses
// as new regions.
// Note: THERE IS NO OPTIMIZATION HERE. It usese whatever PEBS or the profiler has returned.
//  */
// void add_unaccessed_region(vector<REGION_SKD *> regions, uint64_t min_val, uint64_t max_val) {
  
//     return ;
// }

// /* The function add region with access count as 0
// It iwll also take care of merging the regions.
// Current approach:
//      -  Simple count based.
//      - No OTPIMIZATIONS used.
//  */
// void merge_regions_smartly(vector<REGION_SKD *> regions) {
   
//     return;
// }

// int REGION_PEBS_SMART::assign_events_to_regions(vector<AddrEntry *> page_addrs) 
// {
    


//     return 0;
// }