#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using std::string, std::vector;

namespace FileIO {

string read(const fs::path &path);
bool write(const fs::path &path, const string &contents);
bool write(const fs::path &path, const vector<std::uint8_t> &contents);

}
