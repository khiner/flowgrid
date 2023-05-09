#pragma once

#include "nlohmann/json_fwd.hpp"
#include <set>

#include "Store.h"

inline static const fs::path InternalPath = ".flowgrid";
inline static const string FaustDspFileExtension = ".dsp";

struct Project {
    enum Format {
        StateFormat,
        ActionFormat
    };

    static const std::set<string> AllProjectExtensions;

    static bool IsUserProjectPath(const fs::path &);

    static nlohmann::json GetProjectJson(Format format = StateFormat);
    static void SaveEmptyProject();
    static void OpenProject(const fs::path &);
    static bool SaveProject(const fs::path &);
    static void SaveCurrentProject();

    static void RunQueuedActions(bool force_finalize_gesture = false);

    static void Init();

    // Main setter to modify the canonical application state store.
    // _All_ store assignments happen via this method.
    static Patch SetStore(const Store &);

    static void SetHistoryIndex(Count);
};
