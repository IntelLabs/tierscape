

#ifndef MADVISE_TIERED_H
#define MADVISE_TIERED_H


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

#include <bits/stdc++.h>

#include "../utils/model.h"

int push_a_region(REGION_SKD* curr_region, int pid, int dst_virt_tier);
void handle_syscall_error(REGION_SKD* curr_region, int& pid, int& dst_virt_tier);
int open_pagemap_file(int pid);
int close_pagemap_file();

int is_ntier_kernel();


#endif