#ifndef MAPPING_WATERFALL_H
#define MAPPING_WATERFALL_H

#define _POSIX_C_SOURCE 200809L

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

// config.h
#pragma once
extern float SKD_HOT_TH;

class MAPPING_WATERFALL: public MAPPING_BASE{

    public:
    MAPPING_WATERFALL(){

        pr_debug("Mapping Waterfall created. \n");
    }

    int process_regions(vector<REGION_SKD *> *regions);
    


};

// void process_regions_mpl1(vector<REGION_DAMO *> regions);

#endif