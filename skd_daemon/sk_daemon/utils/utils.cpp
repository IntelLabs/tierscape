#include "utils.h"

#include <bits/stdc++.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>


int is_process_running(int pid) {
    struct stat sts;
    char cmd[CHAR_FILE_LEN];
    sprintf(cmd, "/proc/%d", pid);
    while (stat(cmd, &sts) != -1 && errno != ENOENT) {
        //pr_debug("Process still running\n");
        return 1;
    }

    return 0;
}

void free_vector(vector<REGION_SKD*> ans_d) {
    uint64_t len = ans_d.size();

    for (uint64_t i = 0; i < len; i++) {
        if (ans_d[i]) {
            delete ans_d[i];
            ans_d[i] = nullptr;
        }
    }
}

std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, int(*)(FILE*)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

const char* get_process_name_by_pid(const int pid) {
    char* name = (char*)calloc(1024, sizeof(char));
    if (name) {
        sprintf(name, "/proc/%d/cmdline", pid);
        FILE* f = fopen(name, "r");
        if (f) {
            size_t size;
            size = fread(name, sizeof(char), 1024, f);
            if (size > 0) {
                if ('\n' == name[size - 1])
                    name[size - 1] = '\0';
            }
            fclose(f);
        }
    }
    return name;
}


#include <fstream>
#include <sstream>
#include <string>
#include <limits>
#include <vector>

// Structure to hold information about a memory region
struct MemoryRegion {
    std::string permission;
    unsigned long long start_address;
    unsigned long long end_address;
};
