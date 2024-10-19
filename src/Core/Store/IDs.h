#pragma once

#include <unordered_map>

#include "Core/Helper/Path.h"

using ID = unsigned int;

struct IDs {
    static std::unordered_map<StorePath, ID, PathHash> ByPath;
};
