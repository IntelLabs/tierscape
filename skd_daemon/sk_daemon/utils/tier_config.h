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
    const char* pool_manager;
    const char* compressor;
    MEM_TYPE mem_type;
    int backing_store;
    bool is_cpu;
    int tier_latency;
};



// Tier configurations array
static const TierConfigData TIER_CONFIGS[] = {
    // Standard tiers
    {FAST_NODE,  "na", "na", DRAM, FAST_NODE, true, 2},
    {SLOW_NODE,  "na", "na", OPTANE, SLOW_NODE, true, 4},
    
#ifdef ENABLE_NTIER
    // Compressed tiers (commented out - can be enabled as needed)
    {COMPRESSED_TIERS_BASE+0,  "zsmalloc", "lzo", COMPRESSED, FAST_NODE, true, 5},
    {COMPRESSED_TIERS_BASE+1,  "zsmalloc", "zstd", COMPRESSED, FAST_NODE, true, 6},
    {COMPRESSED_TIERS_BASE+2,  "zsmalloc", "zstd", COMPRESSED, SLOW_NODE, true, 7},
#endif
};

// if (config.is_compressed) {
// 			// Create compressed tier using the appropriate constructor
// 			tiers.push_back(new TierInfo(
// 				config.virt_id,
// 				config.pool_manager,
// 				config.compressor,
// 				config.backing_store,
// 				config.is_cpu,
// 				config.tier_latency
// 			));
// 		} else {
// 			// Create standard tier using the simpler constructor
// 			tiers.push_back(new TierInfo(
// 				config.virt_id,
// 				config.mem_type,
// 				config.tier_latency
// 			));
// 		}

#define NUM_TIER_CONFIGS (sizeof(TIER_CONFIGS) / sizeof(TIER_CONFIGS[0]))

// Cost constants
#define DRAM_TIER_COST 10
#define OPTANE_TIER_COST 3

#endif // TIER_CONFIG_H
