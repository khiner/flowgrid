#pragma once

#include <memory>

#include "concurrentqueue.h"

#include "Core/Action/ActionMoment.h"
#include "Core/ActionableComponent.h"
#include "Core/CoreActionHandler.h"
#include "Core/CoreActionProducer.h"
#include "Core/FileDialog/FileDialog.h"
#include "Core/Store/Store.h"
#include "Preferences.h"
#include "ProjectContext.h"
#include "ProjectCore.h"

#include "Core/CoreAction.h"
#include "Core/FileDialog/FileDialogAction.h"
#include "Core/Store/StoreAction.h"
#include "Core/Store/StorePatch.h"
#include "Core/Style/StyleAction.h"
#include "Core/WindowsAction.h"
#include "ProjectAction.h"

// todo just need a little more finagling to finally be done w/ project/app decoupling...
// This is the only remaining project knowledge of anything FlowGrid-specific (non-core).
// It's only needed for
// - defining the `Action::Any` variant,
// - delegating `AppActionType` actions to the `App` component,
#include "FlowGridAction.h"
using AppActionType = Action::FlowGrid::Any;
using AppType = ActionableComponent<AppActionType>;

namespace Action {
// `Any` holds any action type.
//  - Metrics->Project->'Action variant size' shows the byte size of `Action::Any`.
using Any = Combine<Core::Any, Project::Any, FileDialog::Any, Style::Any, Windows::Any, Store::Any, AppActionType>;
using Saved = Filter<Action::IsSaved, Any>;
using NonSaved = Filter<Action::IsNotSaved, Any>;
} // namespace Action

using SavedActionMoment = ActionMoment<Action::Saved>;
using SavedActionMoments = std::vector<SavedActionMoment>;

struct Gesture;

/**
`ProjectState` is the root component of a project, and it fully describes the project state.
It's a structured representation of its underlying store (of type `Store`,
which is composed of an `immer::map<Path, {Type}>` for each stored type).
**Both the `ProjectCore` and `App` components get injected into it by the owning `Project`.**
*/
struct ProjectState : Component {
    ProjectState(TransientStore &store, const ProjectContext &ctx) : Component(store, "Project", ctx) {}

    void FocusDefault() const override {
        for (const auto *c : Children) c->FocusDefault();
    }
    // Overriding to not draw root submenu.
    void DrawWindowsMenu() const override {
        for (const auto *c : Children) c->DrawWindowsMenu();
    }
};

struct StoreHistory;

struct Plottable {
    std::vector<std::string> Labels;
    std::vector<u64> Values;
};

/**
Holds the root `ProjectState` component.
Owns and processes the action queue, store, project history, and other project-level things.
*/
struct Project : ActionableProducer<Action::Any> {
    using CreateApp = std::function<std::unique_ptr<AppType>(AppType::ArgsT)>;

    Project(CreateApp &&);
    ~Project();

    // Find the field whose `Refresh()` should be called in response to a patch with this component ID and op type.
    static Component *FindChanged(ID, const std::vector<PatchOp> &ops);

    void OnApplicationLaunch() const;
    void Tick();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void Draw() const;

    void CommitGesture() const;

    Plottable StorePathChangeFrequencyPlottable() const;

    json GetProjectJson(ProjectFormat) const;

    // Provided queue is drained.
    void ApplyQueuedActions(bool force_commit_gesture = false);
    bool HasGestureActions() const { return !ActiveGestureActions.empty(); }
    const SavedActionMoments &GetGestureActions() const { return ActiveGestureActions; }
    float GestureTimeRemainingSec() const;

    using QueueType = moodycamel::ConcurrentQueue<ActionMoment<ActionType>, moodycamel::ConcurrentQueueDefaultTraits>;
    QueueType Queue{};
    moodycamel::ProducerToken EnqueueToken{Queue};
    moodycamel::ConsumerToken DequeueToken{Queue};
    mutable ActionMoment<ActionType> DequeueActionMoment{};

    mutable Preferences Preferences;
    FileDialog FileDialog{FileDialog::EnqueueFn(SubProducer<FileDialog::ProducedActionType>(*this))};
    CoreActionProducer CoreQ{SubProducer<Action::Core::Any>(*this)};

    mutable SavedActionMoments ActiveGestureActions{}; // uncompressed, uncommitted
    mutable std::optional<fs::path> CurrentProjectPath;
    mutable bool ProjectHasChanges{false}; // todo after store is fully value-oriented, this can be replaced with a comparison of the store and the last saved store.
    mutable bool IsWidgetGesturing{};
    mutable std::string PrevSelectedPath;

    using PathsMoment = std::pair<TimePoint, std::unordered_set<StorePath, PathHash>>;

    // Chronological vector of (unique-field-relative-paths, store-commit-time) pairs for each field that has been updated during the current gesture.
    mutable std::unordered_map<ID, std::vector<PathsMoment>> GestureChangedPaths{};
    // IDs of all fields updated/added/removed during the latest action or undo/redo, mapped to all (field-relative) paths affected in the field.
    // For primitive fields, the paths will consist of only the root path.
    // For container fields, the paths will contain the container-relative paths of all affected elements.
    // All values are appended to `GestureChangedPaths` if the change occurred during a runtime action batch (as opposed to undo/redo, initialization, or project load).
    // `ChangedPaths` is cleared after each action (after refreshing all affected fields), and can thus be used to determine which fields were affected by the latest action.
    // (`LatestChangedPaths` is retained for the lifetime of the application.)
    // These same key IDs are also stored in the `ChangedIds` set, which also includes IDs for all ancestor component of all changed components.
    mutable std::unordered_map<ID, PathsMoment> ChangedPaths;
    // Latest (unique-field-relative-paths, store-commit-time) pair for each field over the lifetime of the application.
    // This is updated by both the forward action pass, and by undo/redo.
    mutable std::unordered_map<ID, PathsMoment> LatestChangedPaths{};
    // IDs of all fields to which `ChangedPaths` are attributed.
    // These are the fields that should have their `Refresh()` called to update their cached values to synchronize with their backing store.
    mutable std::unordered_set<ID> ChangedIds;
    // Components with at least one descendent (excluding itself) updated during the latest action pass.
    mutable std::unordered_set<ID> ChangedAncestorComponentIds;
    mutable std::unordered_map<ID, std::unordered_set<ChangeListener *>> ChangeListenersById;

    ProjectContext Ctx{
        .Preferences = Preferences,
        .FileDialog = FileDialog,
        .Q = CoreQ,

        .RegisterWindow = [this](ID id, bool dock = true) { return Core.Windows.Register(id, dock); },
        .IsDock = [this](ID id) { return Core.Windows.IsDock(id); },
        .IsWindow = [this](ID id) { return Core.Windows.IsWindow(id); },
        .IsWindowVisible = [this](ID id) { return Core.Windows.IsVisible(id); },
        .DrawMenuItem = [this](const Component &c) { Core.Windows.DrawMenuItem(c); },
        .ToggleDemoWindow = [this](ID id) { Q(Action::Windows::ToggleDebug{id}); },

        .GetProjectJson = [this](ProjectFormat format) { return GetProjectJson(format); },
        .GetProjectStyle = [this]() -> const ProjectStyle & { return Core.Style.Project; },

        .RenderMetrics = [this]() { RenderMetrics(); },
        .RenderStorePathChangeFrequency = [this]() { RenderStorePathChangeFrequency(); },

        .UpdateWidgetGesturing = [this]() { UpdateWidgetGesturing(); },
        .LatestUpdateTime = [this](ID id, std::optional<StorePath> relative_path) { return LatestUpdateTime(id, std::move(relative_path)); },

        .IsChanged = [this](ID id) { return ChangedIds.contains(id); },
        .IsDescendentChanged = [this](ID id) { return ChangedAncestorComponentIds.contains(id); },
        .RegisterChangeListener = [this](ChangeListener *listener, ID id) noexcept { ChangeListenersById[id].insert(listener); },
        .UnregisterChangeListener = [this](ChangeListener *listener) noexcept {
            for (auto &[component_id, listeners] : ChangeListenersById) listeners.erase(listener);
            std::erase_if(ChangeListenersById, [](const auto &entry) { return entry.second.empty(); }); },
    };

    mutable PersistentStore S;
    mutable TransientStore _S;
    ProjectState State{_S, Ctx};
    ProjectCore Core{{{&State, "Core"}, SubProducer<ProjectCore::ProducedActionType>(*this)}};

private:
    std::unique_ptr<AppType> App;
    std::unique_ptr<StoreHistory> HistoryPtr;
    StoreHistory &History; // A reference to the above unique_ptr for convenience.

    CoreActionHandler CoreHandler{_S};

    void Open(const fs::path &) const;
    bool Save(const fs::path &) const;

    void SetCurrentProjectPath(const fs::path &) const;
    void OpenStateFormatProject(const fs::path &file_path) const;

    void SetHistoryIndex(u32) const;

    void OpenRecentProjectMenuItem() const;

    void RenderMetrics() const;
    void RenderStorePathChangeFrequency() const;

    std::optional<TimePoint> LatestUpdateTime(ID, std::optional<StorePath> relative_path) noexcept;
    void UpdateWidgetGesturing() const;

    // Refresh the cached values of all fields affected by the patch, and notify all listeners of the affected fields.
    // This is always called immediately after a store commit.
    void RefreshChanged(Patch &&, bool add_to_gesture = false) const;
    // Find and mark fields that are made stale with the provided patch.
    // If `Refresh()` is called on every field marked in `ChangedIds`, the component tree will be fully refreshed.
    // This method also updates the following static fields for monitoring: ChangedAncestorComponentIds, ChangedPaths, LatestChangedPaths
    void MarkAllChanged(Patch &&) const;
    void ClearChanged() const;

    // Overwrite persistent and transient stores with the provided store, and return the resulting patch.
    Patch CheckedCommit(ID base_id) const {
        PersistentStore new_store{_S.Persistent()};
        const auto patch = CreatePatch(S, new_store, base_id);
        S = std::move(new_store);
        _S = S.Transient();
        return patch;
    }
};
