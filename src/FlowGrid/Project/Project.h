#pragma once

#include "Audio/Audio.h"
#include "Core/Action/ActionMenuItem.h"
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
This class fully describes the project at any point in time.
It is a structured representation of its underlying store (of type `Store`,
which is composed of an `immer::map<Path, {Type}>` for each stored type).
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
    static Component *FindChanged(const StorePath &, PatchOpType);

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

    ActionMenuItem<ActionType>
        OpenEmptyMenuItem{*this, Action::Project::OpenEmpty{}, "Cmd+N"},
        ShowOpenDialogMenuItem{*this, Action::Project::ShowOpenDialog{}, "Cmd+O"},
        OpenDefaultMenuItem{*this, Action::Project::OpenDefault{}, "Shift+Cmd+O"},
        SaveCurrentMenuItem{*this, Action::Project::SaveCurrent{}, "Cmd+S"},
        SaveDefaultMenuItem{*this, Action::Project::SaveDefault{}},
        UndoMenuItem{*this, Action::Project::Undo{}, "Cmd+Z"},
        RedoMenuItem{*this, Action::Project::Redo{}, "Shift+Cmd+Z"};

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

    void RenderDebug() const override;

    void ApplyQueuedActions(ActionQueue<ActionType> &queue, bool force_commit_gesture = false, bool ignore_actions = false) const;

protected:
    void Render() const override;

private:
    std::optional<ActionType> ProduceKeyboardAction() const;

    void ApplyPrimitiveAction(const Action::Primitive::Any &) const;
    void ApplyContainerAction(const Action::Container::Any &) const;

    void Open(const fs::path &) const;
    bool Save(const fs::path &) const;

    void OpenStateFormatProject(const fs::path &file_path) const;

    void SetHistoryIndex(u32) const;

    void WindowMenuItem() const;
};
