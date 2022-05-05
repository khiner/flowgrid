#include "file_helpers.h"

#include <filesystem>
#include <fstream>
#include <string>

// TODO handle errors
std::string read_file(const fs::path &path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    const auto size = fs::file_size(path);
    std::string result(size, '\0');
    f.read(result.data(), long(size));
    return result;
}

bool write_file(const fs::path &path, const std::string &contents) {
    std::fstream out_file;
    out_file.open(path, std::ios::out);
    if (out_file) {
        out_file << contents.c_str();
        out_file.close();
        return true;
    }

    return false;
}
