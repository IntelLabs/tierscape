#ifndef MPL2_H
#define MPL2_H


#include "region_base.h"

// --------- Clean up below this line -----------

#include "mapping_base.h"

#include "utils.h"

class REGION_PEBS_FIXED : public REGION_BASE {

    public:
    /* Here we divide the address space in equal regions */
    int init_initial_regions(int pid);
    int assign_events_to_regions(vector<AddrEntry *> page_addrs) ;
    
    int o_pid;
    int fix_regions();
    int fill_regions();
};

#endif