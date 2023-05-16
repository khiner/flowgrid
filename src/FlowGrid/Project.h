#pragma once

#include <__filesystem/path.h>

#include "ProjectConstants.h"

namespace fs = std::filesystem;

struct Project {
    static void Init();

    static bool IsUserProjectPath(const fs::path &);

    static void SaveEmptyProject();
    static void OpenProject(const fs::path &);
    static bool SaveProject(const fs::path &);
    static void SaveCurrentProject();

    static void RunQueuedActions(bool force_finalize_gesture = false);
};