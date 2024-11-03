#pragma once

#include <functional>

#include "nlohmann/json_fwd.hpp"

#include "Core/Primitive/ID.h"

enum class ProjectFormat {
    State,
    Action
};

struct Preferences;
struct ProjectStyle;
struct FileDialog;
struct PrimitiveActionQueuer;

struct Component;

/*
`ProjectContext` is essentially the public slice of a `Project`.
Every component under (and including) the project's root `ProjectState` has access to it.
It doesn't know about any specific `Component` or `Store` (but it may be templated on them in the future).
*/
struct ProjectContext {
    const Preferences &Preferences;
    const FileDialog &FileDialog;
    const PrimitiveActionQueuer &PrimitiveQ;

    const std::function<void(ID)> RegisterWindow;
    const std::function<bool(ID)> IsWindow;
    const std::function<bool(ID)> IsWindowVisible;
    const std::function<void(const Component &)> DrawMenuItem;
    const std::function<void(ID)> ToggleDemoWindow;

    const std::function<nlohmann::json(ProjectFormat)> GetProjectJson;
    const std::function<const ProjectStyle &()> GetProjectStyle;

    const std::function<void()> RenderMetrics;
    const std::function<void()> RenderStorePathChangeFrequency;
};
