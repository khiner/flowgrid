#pragma once

#include <string>
#include <vector>

#include "Helper/Path.h"

namespace FileIO {
std::string read(const fs::path &path);
bool write(const fs::path &path, const std::string &contents);
bool write(const fs::path &path, const std::vector<std::uint8_t> &contents);
} // namespace FileIO
