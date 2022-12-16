#include "File.h"
#include "fstream"

// TODO handle errors

string FileIO::read(const fs::path &path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    const auto size = fs::file_size(path);
    string result(size, '\0');
    f.read(result.data(), long(size));
    return result;
}

bool FileIO::write(const fs::path &path, const string &contents) {
    std::fstream out_file;
    out_file.open(path, std::ios::out);
    if (out_file) {
        out_file << contents;
        out_file.close();
        return true;
    }

    return false;
}

bool FileIO::write(const fs::path &path, const vector<std::uint8_t> &contents) {
    std::fstream out_file(path, std::ios::out | std::ios::binary);
    if (out_file) {
        out_file.write(reinterpret_cast<const char *>(contents.data()), std::streamsize(contents.size()));
        out_file.close();
        return true;
    }

    return false;
}
