#ifndef CORE_H
#define CORE_H


#include "region_base.h"

// --------- Clean up below this line -----------


class REGION_PEBS_SMART : public REGION_BASE {

    public:
    /* Here we divide the address space in equal regions */
    int init_initial_regions(int pid);

    int assign_events_to_regions(vector<AddrEntry *> page_addrs) ;
};

#endif