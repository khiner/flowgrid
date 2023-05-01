#pragma once

#include <__filesystem/path.h>
#include <list>

#include "Config.h"

namespace fs = std::filesystem;

struct AppPreferences {
    AppPreferences();

    bool Write() const;
    bool Clear(); // Clear and re-save default preferences.

    void SetCurrentProjectPath(const fs::path &);

    inline static const fs::path Path = InternalPath / ("Preferences" + PreferencesFileExtension);

    // Saved fields:
    std::list<fs::path> RecentlyOpenedPaths{};
};

extern AppPreferences Preferences;
