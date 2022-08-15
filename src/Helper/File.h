#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using std::string;
using MessagePackBytes = std::vector<std::uint8_t>;

namespace FileIO {

string read(const fs::path &path);
bool write(const fs::path &path, const string &contents);
bool write(const fs::path &path, const MessagePackBytes &contents);

}
