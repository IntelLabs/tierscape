#include "mapping_none.h"

using namespace std;

extern std::atomic<bool> is_program_alive;

extern vector<AddrEntry *> child_addr_vector;
extern int c_count;
extern float SKD_HOT_TH;

int MAPPING_NONE::process_regions(vector<REGION_SKD *> *regions) {
    
    (void)regions;
    return 0;
}
