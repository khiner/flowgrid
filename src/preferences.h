#pragma once

#include <list>
#include <filesystem>
#include "json_type.h"

namespace fs = std::filesystem;

struct Preferences {
    std::list<fs::path> recently_opened_paths;
};

JSON_TYPE(Preferences, recently_opened_paths)

static const fs::path preferences_path = "preferences.flp";
