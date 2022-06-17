#include "FileHelpers.h"

#include <filesystem>
#include <fstream>
#include <vector>

// TODO handle errors
string read_file(const fs::path &path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    const auto size = fs::file_size(path);
    string result(size, '\0');
    f.read(result.data(), long(size));
    return result;
}

bool write_file(const fs::path &path, const string &contents) {
    std::fstream out_file;
    out_file.open(path, std::ios::out);
    if (out_file) {
        out_file << contents.c_str();
        out_file.close();
        return true;
    }

    return false;
}

bool write_file(const fs::path &path, const MessagePackBytes &contents) {
    std::fstream out_file(path, std::ios::out | std::ios::binary);
    if (out_file) {
        out_file.write(reinterpret_cast<const char *>(contents.data()), std::streamsize(contents.size()));
        out_file.close();
        return true;
    }

    return false;
}
