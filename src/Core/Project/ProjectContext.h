#pragma once

#include <functional>

#include "nlohmann/json_fwd.hpp"

#include "Core/Helper/Time.h"
#include "Core/ID.h"

enum class ProjectFormat {
    State,
    Action
};

struct Preferences;
struct ProjectStyle;
struct FileDialog;
struct CoreActionProducer;

struct Component;
struct ChangeListener;

/*
`ProjectContext` is essentially the public slice of a `Project`.
Every component under (and including) the project's root `ProjectState` has access to it.
It doesn't know about any specific `Component` or `Store` (but it may be templated on them in the future).
*/
struct ProjectContext {
    const Preferences &Preferences;
    const FileDialog &FileDialog;
    const CoreActionProducer &Q;

    const std::function<void(ID, bool dock)> RegisterWindow;
    const std::function<bool(ID)> IsDock;
    const std::function<bool(ID)> IsWindow;
    const std::function<bool(ID)> IsWindowVisible;
    const std::function<void(const Component &)> DrawMenuItem;
    const std::function<void(ID)> ToggleDemoWindow;

    const std::function<nlohmann::json(ProjectFormat)> GetProjectJson;
    const std::function<const ProjectStyle &()> GetProjectStyle;

    const std::function<void()> RenderMetrics;
    const std::function<void()> RenderStorePathChangeFrequency;

    const std::function<void()> UpdateWidgetGesturing;
    const std::function<std::optional<TimePoint>(ID, std::optional<StorePath> relative_path)> LatestUpdateTime;
    const std::function<bool(ID)> IsChanged;
    const std::function<bool(ID)> IsDescendentChanged;

    const std::function<void(ChangeListener *, ID)> RegisterChangeListener;
    const std::function<void(ChangeListener *)> UnregisterChangeListener;
};
