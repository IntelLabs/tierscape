#include "region_base.h"
#include "mapping_base.h"

void* th_process_events_and_migrate(int pid, int window_seconds);
void reset_counters();
void migrate_regions(REGION_BASE* region_logic, int pid, int dst_node);
int move_skd_region(REGION_SKD *region_skd, int pid, int dst_node) ;
