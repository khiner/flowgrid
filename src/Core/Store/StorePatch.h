#pragma once

#include "Patch/Patch.h"

struct PersistentStore;

// Create a patch comparing the provided store maps.
Patch CreatePatch(const PersistentStore &, const PersistentStore &, ID base_id);
