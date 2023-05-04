#pragma once

#include "nlohmann/json_fwd.hpp"
#include <set>

#include "Actions.h"
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

    static void Clear();

    // Main setter to modify the canonical application state store.
    // _All_ store assignments happen via this method.
    static Patch SetStore(const Store &);

    static void SetHistoryIndex(Count);

    // todo there's some weird circular dependency type thing going on here.
    //   I should be able to define this inside `Project.h` and not include `Actions.h` here,
    //   but when I do, it compiles but with invisible issues around `Match` not working with `ProjectAction`.
    static void ApplyAction(const ProjectAction &);
};
