#pragma once

#include <__filesystem/path.h>
#include <list>
#include <string>

namespace fs = std::filesystem;

struct AppPreferences {
    inline static const std::string FileExtension = ".flp";
    inline static const fs::path Path = fs::path(".flowgrid") / ("Preferences" + FileExtension);

    AppPreferences();

    bool Write() const;
    bool Clear(); // Clear and re-save default preferences.

    void OnProjectOpened(const fs::path &);

    // Saved fields:
    std::list<fs::path> RecentlyOpenedPaths{};
};

extern AppPreferences Preferences;
