#ifndef UTILS_H
#define UTILS_H

#include "model.h"
#include <atomic>
#include <unordered_set>

std::string exec(const char* cmd);
int is_process_running(int pid);
void free_vector(vector<REGION_SKD *> ans_d);

void populate_percentile_hotness(std::vector<REGION_SKD *> &regions);


const char* get_process_name_by_pid(const int pid);


#endif