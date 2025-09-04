
#ifndef PROCESS_ADD_H
#define PROCESS_ADD_H

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/mman.h>

// consecutive numbers from array
#include<bits/stdc++.h>
#include <vector>

#include "model.h"
#include "utils.h"

using namespace std;

struct hotness_and_entries {
    int hotness;
    uint64_t count_sum;
    float std;
    float mean;
    uint64_t min;
    uint64_t max;
};

hotness_and_entries *get_hotness_region(int start_idx, int end_idx, vector<AddrCountEntry *> a);

void print_ans(vector<REGION_SKD *> ans);
vector<REGION_SKD *> fill_gap_smartly(vector<REGION_SKD *> ans, uint64_t min_val, uint64_t max_val);
vector<REGION_SKD *> collect_regions_sorted_by_size(vector<REGION_SKD *> regions, uint64_t hotness_level);
vector<AddrCountEntry *> sort_count_unique_process(vector<AddrEntry *> page_addrs);
int sort_count_unique_process_mod(vector<AddrEntry *> page_addrs, uint64_t min_val, uint64_t max_val, vector<AddrCountEntry*> *scup);
bool covers_the_val(uint64_t val, REGION_SKD *rd);

address_range get_address_range(int pid);

#endif