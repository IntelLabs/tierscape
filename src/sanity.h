#pragma once

#include "config.h"

// Run all sanity checks. Returns 0 if all pass, -1 on failure.
// Prints clear error messages for each failed check.
int sanity_check_all(const Config& cfg);
