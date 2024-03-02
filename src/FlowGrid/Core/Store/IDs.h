#pragma once

#include <unordered_map>

#include "Helper/Path.h"

using ID = unsigned int;

struct IDs {
    static std::unordered_map<StorePath, ID, PathHash> ByPath;
};
