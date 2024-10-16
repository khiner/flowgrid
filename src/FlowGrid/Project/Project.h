#pragma once

#include "Audio/Audio.h"
#include "Core/Action/ActionMenuItem.h"
#include "Core/Action/ActionableProducer.h"
#include "Core/Action/Actions.h"
#include "Core/ImGuiSettings.h"
#include "Core/Windows.h"
#include "Demo/Demo.h"
#include "FileDialog/FileDialog.h"
#include "Info/Info.h"
#include "ProjectSettings.h"
#include "Style/Style.h"

namespace moodycamel {
struct ConsumerToken;
}

enum ProjectFormat {
    StateFormat,
    ActionFormat
};

struct StoreHistory;

struct Plottable {
    std::vector<std::string> Labels;
    std::vector<u64> Values;
};

struct Project;

/**
`State` fully describes the FlowGrid application state at any point in time.
It's a structured representation of its underlying store (of type `Store`,
which is composed of an `immer::map<Path, {Type}>` for each stored type).
*/
struct State : Component, ActionableProducer<Action::Any> {
    // todo project param is temporary to make it easier to migrate `Component` to hold a `State` component rather than _being_ a `Component`.
    //  It's a bad parent-access pattern.
    //  Instead, `Project` should be further broken up into a `ProjectContext` struct that can be read by components.
    //  This also would be the place to put the state that's currently static in `Component`.
    State(Store &, const PrimitiveActionQueuer &, ActionProducer::EnqueueFn, Project &);
    ~State();

    Project &P;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    struct Debug : DebugComponent, Component::ChangeListener {
        Debug(ComponentArgs &&args, ImGuiWindowFlags flags = WindowFlags_None)
            : DebugComponent(
                  std::move(args), flags,
                  Menu({
                      Menu("Settings", {AutoSelect, LabelMode}),
                      Menu({}), // Need multiple elements to disambiguate vector-of-variants construction from variant construction.
                  })
              ) {
            AutoSelect.RegisterChangeListener(this);
        }
        ~Debug() {
            UnregisterChangeListener(this);
        }

        struct Metrics : Component {
            using Component::Component;

            struct FlowGridMetrics : Component {
                using Component::Component;

                Prop(Bool, ShowRelativePaths, true);

            protected:
                void Render() const override;
            };

            struct ImGuiMetrics : Component {
                using Component::Component;

            protected:
                void Render() const override;
            };

            struct ImPlotMetrics : Component {
                using Component::Component;

            protected:
                void Render() const override;
            };

            Prop(FlowGridMetrics, FlowGrid);
            Prop(ImGuiMetrics, ImGui);
            Prop(ImPlotMetrics, ImPlot);

        protected:
            void Render() const override;
        };

        struct StatePreview : Component {
            using Component::Component;

            Prop(Enum, Format, {"StateFormat", "ActionFormat"}, 1);
            Prop(Bool, Raw);

        protected:
            void Render() const override;
        };

        struct StorePathUpdateFrequency : Component {
            using Component::Component;

        protected:
            void Render() const override;
        };

        struct DebugLog : Component {
            using Component::Component;

        protected:
            void Render() const override;
        };

        struct StackTool : Component {
            using Component::Component;

        protected:
            void Render() const override;
        };

        enum LabelModeType {
            Annotated,
            Raw
        };

        void OnComponentChanged() override;

        Prop_(Enum, LabelMode, "?'Raw' mode shows plain data structures and 'Annotated' mode shows (highlighted) human-readable labels in some cases.\n"
                               "For example, colors are stored as lists with a separate label mapping."
                               "When 'Annotated' mode is enabled, color keys are shown as labels instead of indexes.",
              {"Annotated", "Raw"}, Annotated);
        Prop_(Bool, AutoSelect, "Auto-Select?When enabled, changes to state automatically expand the tree to open the changed field value leaf, closing all other state nodes.\n"
                                "State menu items can only be opened or closed manually if auto-select is disabled.",
              true);

        Prop(StatePreview, StatePreview);
        Prop(StorePathUpdateFrequency, StorePathUpdateFrequency);
        Prop(DebugLog, DebugLog);
        Prop(StackTool, StackTool);
        Prop(Metrics, Metrics);
    };

    ProducerProp(FileDialog, FileDialog);
    ProducerProp(fg::Style, Style);
    ProducerProp(Windows, Windows);
    Prop(ImGuiSettings, ImGuiSettings);
    ProducerProp(Audio, Audio, FileDialog);
    Prop(ProjectSettings, Settings);
    Prop(Info, Info);

    Prop(Demo, Demo, FileDialog);
    Prop(Debug, Debug, WindowFlags_NoScrollWithMouse);

    void RenderDebug() const override;

protected:
    void Render() const override;

private:
    std::optional<ProducedActionType> ProduceKeyboardAction() const;
};

// todo project own an action queue (rather than main), and be typed on the store/action type.
//   It should be agnostic to the the store and root component subtype, provide a `ProjectContext` to the root component,
//   and `State` should not reach into `Project`.

/**
Holds the root `State` component... does project things... (todo)
*/
struct Project : Actionable<Action::Any> {
    Project(Store &, moodycamel::ConsumerToken, const PrimitiveActionQueuer &, State::EnqueueFn);
    ~Project();

    // Find the field whose `Refresh()` should be called in response to a patch with this component ID and op type.
    static Component *FindChanged(ID component_id, const std::vector<PatchOp> &ops);

    void OnApplicationLaunch() const;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void CommitGesture() const;
    void AddGesture(Gesture &&) const;

    Plottable StorePathChangeFrequencyPlottable() const;

    json GetProjectJson(const ProjectFormat) const;

    // Provided queue is drained.
    void ApplyQueuedActions(ActionQueue<ActionType> &, bool force_commit_gesture = false) const;
    bool HasGestureActions() const { return !ActiveGestureActions.empty(); }
    const SavedActionMoments &GetGestureActions() const { return ActiveGestureActions; }
    float GestureTimeRemainingSec() const;

    State::EnqueueFn q;
    const Store &S;
    Store &_S;
    State State;

    ActionMenuItem<ActionType>
        OpenEmptyMenuItem{*this, q, Action::Project::OpenEmpty{}, "Cmd+N"},
        ShowOpenDialogMenuItem{*this, q, Action::Project::ShowOpenDialog{}, "Cmd+O"},
        OpenDefaultMenuItem{*this, q, Action::Project::OpenDefault{}, "Shift+Cmd+O"},
        SaveCurrentMenuItem{*this, q, Action::Project::SaveCurrent{}, "Cmd+S"},
        SaveDefaultMenuItem{*this, q, Action::Project::SaveDefault{}},
        UndoMenuItem{*this, q, Action::Project::Undo{}, "Cmd+Z"},
        RedoMenuItem{*this, q, Action::Project::Redo{}, "Shift+Cmd+Z"};

    const Menu MainMenu{
        {
            Menu(
                "File",
                {
                    OpenEmptyMenuItem,
                    ShowOpenDialogMenuItem,
                    [this]() { OpenRecentProjectMenuItem(); },
                    OpenDefaultMenuItem,
                    SaveCurrentMenuItem,
                    SaveDefaultMenuItem,
                }
            ),
            Menu(
                "Edit",
                {
                    UndoMenuItem,
                    RedoMenuItem,
                }
            ),
            [this] { return WindowMenuItem(); },
        },
        true
    };

    std::unique_ptr<StoreHistory> HistoryPtr;
    StoreHistory &History; // A reference to the above unique_ptr for convenience.

private:
    std::unique_ptr<moodycamel::ConsumerToken> DequeueToken;
    mutable ActionMoment<ActionType> DequeueActionMoment{};

    mutable SavedActionMoments ActiveGestureActions{}; // uncompressed, uncommitted
    mutable std::optional<fs::path> CurrentProjectPath;
    mutable bool ProjectHasChanges{false}; // todo after store is fully value-oriented, this can be replaced with a comparison of the store and the last saved store.
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
    void WindowMenuItem() const;

    // Refresh the cached values of all fields affected by the patch, and notify all listeners of the affected fields.
    // This is always called immediately after a store commit.
    void RefreshChanged(Patch &&, bool add_to_gesture = false) const;
    // Find and mark fields that are made stale with the provided patch.
    // If `Refresh()` is called on every field marked in `ChangedIds`, the component tree will be fully refreshed.
    // This method also updates the following static fields for monitoring: ChangedAncestorComponentIds, ChangedPaths, LatestChangedPaths
    void MarkAllChanged(Patch &&) const;
    void ClearChanged() const;
};
