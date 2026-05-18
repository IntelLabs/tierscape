#pragma once

#include "region.h"
#include <vector>

// Classify regions by hotness percentile.
//
//   hot_percentile = 25 means: pages whose hotness sits below the
//   25th percentile go to cold_node; all others stay on hot_node.
//
// Sets region.target_node for every input region.
void classify_regions(std::vector<Region>& regions,
                      float hot_percentile,
                      int hot_node,
                      int cold_node);
