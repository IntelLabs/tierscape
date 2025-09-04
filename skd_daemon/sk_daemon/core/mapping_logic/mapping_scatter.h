
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
class MAPPING_SCATTER: public MAPPING_BASE{

    public:
    MAPPING_SCATTER(){
        pr_debug("MAPPING_SCATTER created\n");
    }
    int process_regions(vector<REGION_SKD *> *regions);

};

// void process_regions_mpl1(vector<REGION_DAMO *> regions);