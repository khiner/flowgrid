#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

std::string read_file(const fs::path &path);
bool write_file(const fs::path &path, const std::string &contents);
