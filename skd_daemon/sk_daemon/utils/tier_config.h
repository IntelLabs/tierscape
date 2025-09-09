#ifndef TIER_CONFIG_H
#define TIER_CONFIG_H

// Include the model header to use TierInfo class
#include "model.h"

// Helper function to create TierInfo instances from configuration
// This function will be implemented in tier_utils.cpp
std::vector<TierInfo*> create_configured_tiers();

// Configuration data for creating tiers
struct TierConfigData {
    int virt_id;
    bool is_compressed;
    const char* pool_manager;
    const char* compressor;
    MEM_TYPE mem_type;
    MEM_TYPE backing_store;
    bool is_cpu;
    int tier_latency;
};

// Tier configurations array
static const TierConfigData TIER_CONFIGS[] = {
    // Standard tiers
    {0, false, "na", "na", DRAM, DRAM, true, 2},
    {1, false, "na", "na", OPTANE, OPTANE, true, 4},
    
#ifdef ENABLE_NTIER
// sdf
    // Compressed tiers (commented out - can be enabled as needed)
    {COMPRESSED_TIERS_BASED+0, true, "zsmalloc", "lzo", COMPRESSED, DRAM, true, 5},
    {COMPRESSED_TIERS_BASED+2, true, "zsmalloc", "zstd", COMPRESSED, DRAM, true, 6},
    {COMPRESSED_TIERS_BASED+1, true, "zsmalloc", "zstd", COMPRESSED, OPTANE, true, 7},
    {COMPRESSED_TIERS_BASED+3, true, "zbud", "zstd", COMPRESSED, DRAM, true, 8},
#endif
};

#define NUM_TIER_CONFIGS (sizeof(TIER_CONFIGS) / sizeof(TIER_CONFIGS[0]))

// Cost constants
#define DRAM_TIER_COST 10
#define OPTANE_TIER_COST 3

#endif // TIER_CONFIG_H
