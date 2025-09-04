
#ifndef MAPPING_HEMEM_H
#define MAPPING_HEMEM_H

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


extern float SKD_HOT_TH;

extern std::atomic<bool> is_program_alive;

extern vector<AddrEntry*> child_addr_vector;
extern int c_count;

extern vector<float> percentile_val_arr, percentile_arr;

class MAPPING_HEMEM: public MAPPING_BASE{

    public:
    MAPPING_HEMEM(){
        
        pr_debug("Mapping HEMEM created.\n" );
    }
    int process_regions(vector<REGION_SKD *> *regions);
    

};

#endif