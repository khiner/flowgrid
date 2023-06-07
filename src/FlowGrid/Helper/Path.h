#pragma once

#include <__filesystem/path.h>

namespace fs = std::filesystem;

struct PathHash {
    auto operator()(const fs::path &p) const noexcept { return fs::hash_value(p); }
};
