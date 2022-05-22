#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;
using std::string;

using MessagePackBytes = std::vector<std::uint8_t>;

string read_file(const fs::path &path);
bool write_file(const fs::path &path, const string &contents);
bool write_file(const fs::path &path, const MessagePackBytes &contents);
