#pragma once

#include "Audio/Audio.h"
#include "Core/Action/ActionQueue.h"
#include "Core/Action/ActionableProducer.h"
#include "Core/Action/Actions.h"
#include "Core/ImGuiSettings.h"
#include "Core/Windows.h"
#include "Demo/Demo.h"
#include "FileDialog/FileDialog.h"
#include "Info/Info.h"
#include "ProjectSettings.h"
#include "Style/Style.h"

#include "Core/Container/ContainerAction.h"
#include "Core/Primitive/PrimitiveAction.h"

enum ProjectFormat {
    StateFormat,
    ActionFormat
};

struct StoreHistory;

struct Plottable {
    std::vector<std::string> Labels;
    std::vector<u64> Values;
};

/**
 * This class fully describes the project at any point in time.
 * An immutable reference to the single source-of-truth project state `const Project &project` is defined at the bottom of this file.
 */
struct Project : Component, ActionableProducer<Action::Any> {
    Project(Store &, PrimitiveActionQueuer &, ActionProducer::Enqueue);
    ~Project();

    // A `ProjectComponent` is a `Component` that can cast the `Root` component pointer to its true `Project` type.
    struct ProjectComponent : Component {
        using Component::Component;

        const Project &GetProject() const { return static_cast<const Project &>(*Root); }
    };

    // Refresh the cached values of all fields affected by the patch, and notify all listeners of the affected fields.
    // This is always called immediately after a store commit.
    static void RefreshChanged(const Patch &, bool add_to_gesture = false);

    inline static void ClearChanged() noexcept {
        ChangedPaths.clear();
        ChangedIds.clear();
        ChangedAncestorComponentIds.clear();
    }

    // Find and mark fields that are made stale with the provided patch.
    // If `Refresh()` is called on every field marked in `ChangedIds`, the component tree will be fully refreshed.
    // This method also updates the following static fields for monitoring: ChangedAncestorComponentIds, ChangedPaths, LatestChangedPaths
    static void MarkAllChanged(const Patch &);

    // Find the field whose `Refresh()` should be called in response to a patch with this path and op type.
    static Component *FindChanged(const StorePath &, PatchOp::Type);

    void OpenRecentProjectMenuItem() const;

    void OnApplicationLaunch() const;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void CommitGesture() const;
    Plottable StorePathChangeFrequencyPlottable() const;

    json GetProjectJson(const ProjectFormat) const;

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

        struct Metrics : ProjectComponent {
            using ProjectComponent::ProjectComponent;

            struct FlowGridMetrics : ProjectComponent {
                using ProjectComponent::ProjectComponent;

                Prop(Bool, ShowRelativePaths, true);

            protected:
                void Render() const override;
            };

            struct ImGuiMetrics : ProjectComponent {
                using ProjectComponent::ProjectComponent;

            protected:
                void Render() const override;
            };

            struct ImPlotMetrics : ProjectComponent {
                using ProjectComponent::ProjectComponent;

            protected:
                void Render() const override;
            };

            Prop(FlowGridMetrics, FlowGrid);
            Prop(ImGuiMetrics, ImGui);
            Prop(ImPlotMetrics, ImPlot);

        protected:
            void Render() const override;
        };

        struct ProjectPreview : ProjectComponent {
            using ProjectComponent::ProjectComponent;

            Prop(Enum, Format, {"StateFormat", "ActionFormat"}, 1);
            Prop(Bool, Raw);

        protected:
            void Render() const override;
        };

        struct StorePathUpdateFrequency : ProjectComponent {
            using ProjectComponent::ProjectComponent;

        protected:
            void Render() const override;
        };

        struct DebugLog : ProjectComponent {
            using ProjectComponent::ProjectComponent;

        protected:
            void Render() const override;
        };

        struct StackTool : ProjectComponent {
            using ProjectComponent::ProjectComponent;

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

        Prop(ProjectPreview, ProjectPreview);
        Prop(StorePathUpdateFrequency, StorePathUpdateFrequency);
        Prop(DebugLog, DebugLog);
        Prop(StackTool, StackTool);
        Prop(Metrics, Metrics);
    };

    std::unique_ptr<StoreHistory> HistoryPtr;
    StoreHistory &History; // A reference to the above unique_ptr for convenience.

    ProducerProp(FileDialog, FileDialog);
    ProducerProp(fg::Style, Style);
    ProducerProp(Windows, Windows);
    Prop(ImGuiSettings, ImGuiSettings);
    ProducerProp(Audio, Audio, FileDialog);
    Prop(ProjectSettings, Settings);
    Prop(Info, Info);

    Prop(Demo, Demo, FileDialog);
    Prop(Debug, Debug, WindowFlags_NoScrollWithMouse);

    const Menu MainMenu{
        {
            Menu("File", {Action::Project::OpenEmpty::MenuItem, Action::Project::ShowOpenDialog::MenuItem, [this]() { OpenRecentProjectMenuItem(); }, Action::Project::OpenDefault::MenuItem, Action::Project::SaveCurrent::MenuItem, Action::Project::SaveDefault::MenuItem}),
            Menu("Edit", {Action::Project::Undo::MenuItem, Action::Project::Redo::MenuItem}),
            [this] { return WindowMenuItem(); },
        },
        true
    };

    void RenderDebug() const override;

    void ApplyQueuedActions(ActionQueue<ActionType> &queue, bool force_commit_gesture = false, bool ignore_actions = false) const;

protected:
    void Render() const override;

private:
    void ApplyPrimitiveAction(const Action::Primitive::Any &) const;
    void ApplyContainerAction(const Action::Container::Any &) const;

    void Open(const fs::path &) const;
    bool Save(const fs::path &) const;

    void OpenStateFormatProject(const fs::path &file_path) const;

    void SetHistoryIndex(u32) const;

    void WindowMenuItem() const;
};

/**
Declare global read-only accessor for the canonical state instance `project`.

`project` is a read-only structured representation of its underlying store (of type `Store`, which itself is an `immer::map<Path, PrimitiveVariant>`).
It provides a complete nested struct representation of the state, along with additional metadata about each state member, such as its `Path`/`ID`/`Name`/`Info`.
Basically, it contains all data for each state member except its _actual value_ (a `Primitive`, struct of `Primitive`s, or collection of either).
(Actually, each primitive leaf value is cached on its respective `Field`, but this is a technicality - the `Store` is conceptually the source of truth.)

`project` has an immutable assignment operator, which return a modified copy of the `Store` value resulting from applying the assignment to the provided `Store`.
(Note that this is only _conceptually_ a copy - see the [application architecture readme section](https://github.com/khiner/flowgrid#application-architecture) for more details.)
*/
extern const Project &project;
