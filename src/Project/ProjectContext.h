#pragma once

#include <functional>

#include "nlohmann/json_fwd.hpp"

#include "Preferences.h"

enum class ProjectFormat {
    State,
    Action
};

/*
`ProjectContext` is essentially the public slice of a `Project`.
Every component under (and including) the project's root `State` has access to it.
It doesn't know about any specific `State` or `Store` (but it may be templated on them in the future).
*/
struct ProjectContext {
    const Preferences &Preferences;
    const std::function<nlohmann::json(ProjectFormat)> GetProjectJson;
    const std::function<void()> RenderMetrics;
    const std::function<void()> RenderStorePathChangeFrequency;
};
