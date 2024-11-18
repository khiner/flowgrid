#pragma once

#include "Core/ActionableComponent.h"
#include "Core/Demo/Demo.h"
#include "Core/FileDialog/FileDialogAction.h"
#include "Core/ImGuiSettings.h"
#include "Core/Info/Info.h"
#include "Core/Style/Style.h"
#include "Core/Windows.h"

#include "ProjectSettings.h"

namespace Action {
namespace ProjectCore {
using Any = Action::Combine<Action::Windows::Any, Action::Style::Any>;
} // namespace ProjectCore
} // namespace Action

/**
Handles core project state underlying any project.
*/
struct ProjectCore : ActionableComponent<Action::ProjectCore::Any, Action::Combine<Action::ProjectCore::Any, Action::FileDialog::Any>> {
    using ActionableComponent::ActionableComponent;
    ProjectCore(ArgsT &&);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void Dock(ID *) const override;
    void FocusDefault() const override;

    struct Debug : DebugComponent, ChangeListener {
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

        void DrawWindowsMenu() const override;

        struct Metrics : Component {
            using Component::Component;

            struct ProjectMetrics : Component {
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

            Prop(ProjectMetrics, Project);
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

    ProducerProp(Windows, Windows);
    Prop(ImGuiSettings, ImGuiSettings);
    ProducerProp(flowgrid::Style, Style);
    Prop(ProjectSettings, Settings);
    Prop(Info, Info);
    ProducerProp(Demo, Demo);
    Prop(Debug, Debug, WindowFlags_NoScrollWithMouse);

    void RenderDebug() const override;
};
