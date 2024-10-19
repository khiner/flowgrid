#pragma once

#include <list>
#include <string>

#include "Helper/Path.h"

struct Preferences {
    inline static const std::string FileExtension = ".flp";
    inline static const fs::path
        Path = fs::path(".flowgrid") / ("Preferences" + FileExtension),
        // todo thinking of digging into grammars' `config.json` files to automatically find the supported file extensions...
        TreeSitterGrammarsPath = fs::path("..") / "lib" / "tree-sitter-grammars",
        // todo recursively copy `queries` dir to build dir in CMake.
        TreeSitterQueriesPath = fs::path("..") / "src" / "FlowGrid" / "Project" / "TextEditor" / "queries";

    Preferences();

    bool Write() const;
    bool Clear(); // Clear and re-save default preferences.

    void OnProjectOpened(const fs::path &);

    // Saved fields:
    std::list<fs::path> RecentlyOpenedPaths{};
    fs::path TreeSitterConfigPath{fs::path{"~"} / "Library" / "Application Support" / "tree-sitter" / "config.json"};
};
