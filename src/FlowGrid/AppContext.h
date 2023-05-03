#pragma once

#include "Config.h"
#include "Store.h"

#include "nlohmann/json_fwd.hpp"

struct Context {
    static bool IsUserProjectPath(const fs::path &);

    nlohmann::json GetProjectJson(ProjectFormat format = StateFormat);
    void SaveEmptyProject();
    void OpenProject(const fs::path &);
    bool SaveProject(const fs::path &);
    void SaveCurrentProject();

    void RunQueuedActions(bool force_finalize_gesture = false);
    bool ActionAllowed(ID) const;
    bool ActionAllowed(const Action &) const;
    bool ActionAllowed(const EmptyAction &) const;

    void Clear();

    // Main setter to modify the canonical application state store.
    // _All_ store assignments happen via this method.
    Patch SetStore(const Store &);

    void SetHistoryIndex(Count);

public:
    bool ProjectHasChanges{false};

private:
    void ApplyAction(const ProjectAction &);

    void SetCurrentProjectPath(const fs::path &);

    std::optional<fs::path> CurrentProjectPath;
};

extern Context c; // Context global, initialized in `main.cpp`.
