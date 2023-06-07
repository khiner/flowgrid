#pragma once

#include <vector>

#include "Core/Primitive.h"
#include "Helper/Path.h"

using StoreEntry = std::pair<StorePath, Primitive>;
using StoreEntries = std::vector<StoreEntry>;
