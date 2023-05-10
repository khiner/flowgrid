#pragma once

#include "nlohmann/json_fwd.hpp"

#include "ProjectConstants.h"
#include "Store.h"

struct Project {
    static bool IsUserProjectPath(const fs::path &);

    static nlohmann::json GetProjectJson(ProjectFormat format = StateFormat);
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
