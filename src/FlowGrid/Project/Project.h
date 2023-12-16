#pragma once

#include "Audio/Audio.h"
#include "Core/Action/Actionable.h"
#include "Core/Action/Actions.h"
#include "Core/ImGuiSettings.h"
#include "Demo/Demo.h"
#include "FileDialog/FileDialog.h"
#include "Info/Info.h"
#include "ProjectContext.h"
#include "ProjectSettings.h"

enum ProjectFormat {
    StateFormat,
    ActionFormat
};

/**
 * This class fully describes the project at any point in time.
 * An immutable reference to the single source-of-truth project state `const Project &project` is defined at the bottom of this file.
 */
struct Project : Component, Actionable<Action::Any> {
    Project(Store &);

    static void OpenRecentProjectMenuItem();

    void OnApplicationLaunch() const;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    json GetProjectJson(const ProjectFormat) const;

    struct Debug : DebugComponent, Field::ChangeListener {
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
            Field::UnregisterChangeListener(this);
        }

        struct ProjectDebugChild : Component {
            using Component::Component;

            const Debug *GetDebug() const { return static_cast<const Debug *>(Parent); }
            const Project *GetProject() const { return GetDebug()->GetProject(); }
        };

        struct Metrics : ProjectDebugChild {
            using ProjectDebugChild::ProjectDebugChild;

            struct ProjectMetricsChild : Component {
                using Component::Component;
                const Metrics *GetMetrics() const { return static_cast<const Metrics *>(Parent); }
                const Project *GetProject() const { return GetMetrics()->GetProject(); }
            };

            struct FlowGridMetrics : ProjectMetricsChild {
                using ProjectMetricsChild::ProjectMetricsChild;

                Prop(Bool, ShowRelativePaths, true);

            protected:
                void Render() const override;
            };

            struct ImGuiMetrics : ProjectMetricsChild {
                using ProjectMetricsChild::ProjectMetricsChild;

            protected:
                void Render() const override;
            };

            struct ImPlotMetrics : ProjectMetricsChild {
                using ProjectMetricsChild::ProjectMetricsChild;

            protected:
                void Render() const override;
            };

            Prop(FlowGridMetrics, FlowGrid);
            Prop(ImGuiMetrics, ImGui);
            Prop(ImPlotMetrics, ImPlot);

        protected:
            void Render() const override;
        };

        struct ProjectPreview : ProjectDebugChild {
            using ProjectDebugChild::ProjectDebugChild;

            Prop(Enum, Format, {"StateFormat", "ActionFormat"}, 1);
            Prop(Bool, Raw);

        protected:
            void Render() const override;
        };

        // StateMemoryEditor, WindowFlags_NoScrollbar
        struct StorePathUpdateFrequency : ProjectDebugChild {
            using ProjectDebugChild::ProjectDebugChild;

        protected:
            void Render() const override;
        };

        struct DebugLog : ProjectDebugChild {
            using ProjectDebugChild::ProjectDebugChild;

        protected:
            void Render() const override;
        };

        struct StackTool : ProjectDebugChild {
            using ProjectDebugChild::ProjectDebugChild;

        protected:
            void Render() const override;
        };

        enum LabelModeType {
            Annotated,
            Raw
        };

        const Project *GetProject() const { return static_cast<const Project *>(Parent); }

        void OnFieldChanged() override;

        Prop_(Enum, LabelMode, "?'Raw' mode shows plain data structures and 'Annotated' mode shows (highlighted) human-readable labels in some cases.\n"
                               "For example, colors are stored as lists with a separate label mapping."
                               "When 'Annotated' mode is enabled, color keys are shown as labels instead of indexes.",
              {"Annotated", "Raw"}, Annotated);
        Prop_(Bool, AutoSelect, "Auto-Select?When enabled, changes to state automatically expand the tree to open the changed field value leaf, closing all other state nodes.\n"
                                "State menu items can only be opened or closed manually if auto-select is disabled.",
              true);

        Prop(ProjectPreview, ProjectPreview);
        // Prop(StateMemoryEditor, StateMemoryEditor);
        Prop(StorePathUpdateFrequency, StorePathUpdateFrequency);
        Prop(DebugLog, DebugLog);
        Prop(StackTool, StackTool);
        Prop(Metrics, Metrics);
    };

    Prop(ProjectContext, Context);
    Prop(ImGuiSettings, ImGuiSettings);
    Prop(Audio, Audio);
    Prop(ProjectSettings, Settings);
    Prop(FileDialog, FileDialog);
    Prop(Info, Info);

    Prop(Demo, Demo);
    Prop(Debug, Debug, WindowFlags_NoScrollWithMouse);

    const Menu MainMenu{
        {
            Menu("File", {Action::Project::OpenEmpty::MenuItem, Action::Project::ShowOpenDialog::MenuItem, OpenRecentProjectMenuItem, Action::Project::OpenDefault::MenuItem, Action::Project::SaveCurrent::MenuItem, Action::Project::SaveDefault::MenuItem}),
            Menu("Edit", {Action::Project::Undo::MenuItem, Action::Project::Redo::MenuItem}),
            [this] { return WindowMenuItem(); },
        },
        true
    };

    void RenderDebug() const override;

protected:
    void Render() const override;

private:
    void Open(const fs::path &) const;
    bool Save(const fs::path &) const;

    void OpenStateFormatProject(const fs::path &file_path) const;

    void SetHistoryIndex(u32) const;

    void WindowMenuItem() const;
};

void RunQueuedActions(const Project &project, Store &, bool force_commit_gesture = false, bool ignore_actions = false);

/**
Declare global read-only accessor for the canonical state instance `project`.

`project` is a read-only structured representation of its underlying store (of type `Store`, which itself is an `immer::map<Path, Primitive>`).
It provides a complete nested struct representation of the state, along with additional metadata about each state member, such as its `Path`/`ID`/`Name`/`Info`.
Basically, it contains all data for each state member except its _actual value_ (a `Primitive`, struct of `Primitive`s, or collection of either).
(Actually, each primitive leaf value is cached on its respective `Field`, but this is a technicality - the `Store` is conceptually the source of truth.)

`project` has an immutable assignment operator, which return a modified copy of the `Store` value resulting from applying the assignment to the provided `Store`.
(Note that this is only _conceptually_ a copy - see the [application architecture readme section](https://github.com/khiner/flowgrid#application-architecture) for more details.)
*/
extern const Project &project;
