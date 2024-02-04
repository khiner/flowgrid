#include "File.h"

#include <fstream>
#include <optional>

#ifdef _WIN32
#include <shlobj.h> // for SHGetFolderPathW
#include <windows.h>
#else
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#endif

static std::optional<fs::path> GetHomeDir() {
#ifdef _WIN32
    wchar_t path[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_PROFILE, nullptr, 0, path) == S_OK) return fs::path(path);
    return std::nullopt;
#else
    const char *home_dir;
    if ((home_dir = getenv("HOME")) == nullptr) home_dir = getpwuid(getuid())->pw_dir;
    if (home_dir != nullptr) return fs::path(home_dir);
    return std::nullopt;
#endif
}

static fs::path ExpandPath(const fs::path &path) {
    if (*path.begin() != "~") return path;

    const auto home_dir = GetHomeDir();
    if (!home_dir.has_value()) throw std::runtime_error("Unable to find the home directory.");

    // Create a relative path, skipping the first element ("~").
    fs::path relative_path;
    for (auto it = ++path.begin(); it != path.end(); ++it) relative_path /= *it;

    return *home_dir / relative_path;
}

std::string FileIO::read(const fs::path &path) {
    const fs::path full_path = ExpandPath(path);
    std::ifstream f(full_path, std::ios::in | std::ios::binary);
    const auto size = fs::file_size(full_path);
    std::string result(size, '\0');
    f.read(result.data(), long(size));
    return result;
}

bool FileIO::write(const fs::path &path, std::string_view contents) {
    std::fstream out_file;
    out_file.open(path, std::ios::out);
    if (out_file) {
        out_file << contents;
        out_file.close();
        return true;
    }

    return false;
}

bool FileIO::write(const fs::path &path, const std::vector<std::uint8_t> &contents) {
    std::fstream out_file(path, std::ios::out | std::ios::binary);
    if (out_file) {
        out_file.write(reinterpret_cast<const char *>(contents.data()), std::streamsize(contents.size()));
        out_file.close();
        return true;
    }

    return false;
}
