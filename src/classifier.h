#pragma once

#include "region.h"
#include <vector>

// Classify regions as hot or cold based on percentile threshold.
// Sets region.target_node for each region.
// hot_percentile: regions at or above this percentile get hot_node.
void classify_regions(std::vector<Region>& regions,
                      float hot_percentile,
                      int hot_node,
                      int cold_node);
