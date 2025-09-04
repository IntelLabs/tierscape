
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
#include <vector>

#include "addr_space_util.h"

#include "utils.h"
#include "model.h"

#include "mapping_base.h"
class MAPPING_GOOGLE: public MAPPING_BASE{

    public:
    MAPPING_GOOGLE(){
        pr_debug("MAPPING_GOOGLE created\n");
    }
    int process_regions(vector<REGION_SKD *> *regions);
    void print_regions_info(vector<REGION_SKD *> regions,int hotness_threshold);

};

// void process_regions_mpl1(vector<REGION_DAMO *> regions);