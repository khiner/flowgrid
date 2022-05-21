#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using std::string;

string read_file(const fs::path &path);
bool write_file(const fs::path &path, const string &contents);
