#pragma once

#include "concurrentqueue.h"

#include "Core/Action/ActionableProducer.h"
#include "Core/Action/Actions.h"
#include "Core/FileDialog/FileDialog.h"
#include "Core/Primitive/PrimitiveActionQueuer.h"
#include "Core/Store/Store.h"

#include "Preferences.h"
#include "ProjectContext.h"
#include "ProjectState.h"

struct StoreHistory;

struct Plottable {
    std::vector<std::string> Labels;
    std::vector<u64> Values;
};

/**
Holds the root `ProjectState` component.
Owns and processes the action queue, store, project history, and other project-level things.

todo project templated on (StoreType, AppComponentType, AppActionType).
  holding a root `ProjectState` that is in turn holds an AppComponentType and ProjectCore
*/
struct Project : ActionableProducer<Action::Any> {
    Project();
    ~Project();

    // Find the field whose `Refresh()` should be called in response to a patch with this component ID and op type.
    static Component *FindChanged(ID, const std::vector<PatchOp> &ops);

    void OnApplicationLaunch() const;
    void Tick();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void Draw() const;

    void CommitGesture() const;
    void AddGesture(Gesture &&) const;

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
    FileDialog FileDialog{FileDialog::EnqueueFn(CreateProducer<FileDialog::ProducedActionType>())};
    PrimitiveActionQueuer PrimitiveQ{CreateProducer<PrimitiveActionQueuer::ActionType>()};

    ProjectContext ProjectContext{
        .Preferences = Preferences,
        .FileDialog = FileDialog,
        .PrimitiveQ = PrimitiveQ,

        .RegisterWindow = [this](ID id, bool dock = true) { return State.Core.Windows.Register(id, dock); },
        .IsDock = [this](ID id) { return State.Core.Windows.IsDock(id); },
        .IsWindow = [this](ID id) { return State.Core.Windows.IsWindow(id); },
        .IsWindowVisible = [this](ID id) { return State.Core.Windows.IsVisible(id); },
        .DrawMenuItem = [this](const Component &c) { State.Core.Windows.DrawMenuItem(c); },
        .ToggleDemoWindow = [this](ID id) { Q(Action::Windows::ToggleDebug{id}); },

        .GetProjectJson = [this](ProjectFormat format) { return GetProjectJson(format); },
        .GetProjectStyle = [this]() -> const ProjectStyle & { return State.Core.Style.Project; },

        .RenderMetrics = [this]() { RenderMetrics(); },
        .RenderStorePathChangeFrequency = [this]() { RenderStorePathChangeFrequency(); },
    };

    mutable Store _S;
    const Store &S{_S};
    ProjectState State{_S, CreateProducer<ProjectState::ProducedActionType>(), ProjectContext};

private:
    std::unique_ptr<StoreHistory> HistoryPtr;
    StoreHistory &History; // A reference to the above unique_ptr for convenience.

    mutable SavedActionMoments ActiveGestureActions{}; // uncompressed, uncommitted
    mutable std::optional<fs::path> CurrentProjectPath;
    mutable bool ProjectHasChanges{false}; // todo after store is fully value-oriented, this can be replaced with a comparison of the store and the last saved store.
    mutable std::string PrevSelectedPath;

    // Chronological vector of (unique-field-relative-paths, store-commit-time) pairs for each field that has been updated during the current gesture.
    mutable std::unordered_map<ID, std::vector<Component::PathsMoment>> GestureChangedPaths{};
    // IDs of all fields updated/added/removed during the latest action or undo/redo, mapped to all (field-relative) paths affected in the field.
    // For primitive fields, the paths will consist of only the root path.
    // For container fields, the paths will contain the container-relative paths of all affected elements.
    // All values are appended to `GestureChangedPaths` if the change occurred during a runtime action batch (as opposed to undo/redo, initialization, or project load).
    // `ChangedPaths` is cleared after each action (after refreshing all affected fields), and can thus be used to determine which fields were affected by the latest action.
    // (`LatestChangedPaths` is retained for the lifetime of the application.)
    // These same key IDs are also stored in the `ChangedIds` set, which also includes IDs for all ancestor component of all changed components.
    mutable std::unordered_map<ID, Component::PathsMoment> ChangedPaths;

    void Open(const fs::path &) const;
    bool Save(const fs::path &) const;

    void SetCurrentProjectPath(const fs::path &) const;
    void OpenStateFormatProject(const fs::path &file_path) const;

    void SetHistoryIndex(u32) const;

    void OpenRecentProjectMenuItem() const;

    void RenderMetrics() const;
    void RenderStorePathChangeFrequency() const;

    // Refresh the cached values of all fields affected by the patch, and notify all listeners of the affected fields.
    // This is always called immediately after a store commit.
    void RefreshChanged(Patch &&, bool add_to_gesture = false) const;
    // Find and mark fields that are made stale with the provided patch.
    // If `Refresh()` is called on every field marked in `ChangedIds`, the component tree will be fully refreshed.
    // This method also updates the following static fields for monitoring: ChangedAncestorComponentIds, ChangedPaths, LatestChangedPaths
    void MarkAllChanged(Patch &&) const;
    void ClearChanged() const;

    Patch CreatePatch();
};
