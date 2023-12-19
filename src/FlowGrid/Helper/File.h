#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace FileIO {
std::string read(const fs::path &);
bool write(const fs::path &, const std::string_view contents);
bool write(const fs::path &, const std::vector<std::uint8_t> &contents);
} // namespace FileIO
