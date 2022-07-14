#pragma once

#include <list>
#include <filesystem>

#include "JsonType.h"

namespace fs = std::filesystem;

struct Preferences {
    std::list<fs::path> recently_opened_paths;
};

JsonType(Preferences, recently_opened_paths)
