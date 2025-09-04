#ifndef MAPPING_ILP_H
#define MAPPING_ILP_H

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
class MAPPING_ILP: public MAPPING_BASE{

    public:
    int process_regions(vector<REGION_SKD *> *regions);
    int start_ilp_server();

    MAPPING_ILP(){
        pr_debug("MAPPING_ILP created\n");
        start_ilp_server();

    }

};


#endif
