#pragma once

#include <list>
#include <string>

#include "Helper/Path.h"

struct ApplicationPreferences {
    inline static const std::string FileExtension = ".flp";
    inline static const fs::path Path = fs::path(".flowgrid") / ("Preferences" + FileExtension);
    // todo thinking of using this to automatically find the supported file extensions...
    // inline static const fs::path TreeSitterGrammarsPath = fs::path("..") / "lib" / "tree-sitter-grammars";

    ApplicationPreferences();

    bool Write() const;
    bool Clear(); // Clear and re-save default preferences.

    void OnProjectOpened(const fs::path &);

    // Saved fields:
    std::list<fs::path> RecentlyOpenedPaths{};
    fs::path TreeSitterConfigPath{fs::path{"~"} / "Library" / "Application Support" / "tree-sitter" / "config.json"};
};

extern ApplicationPreferences Preferences;
