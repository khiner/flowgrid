#pragma once

#include "Core/Action/ActionableProducer.h"
#include "Core/Action/Actions.h"
#include "Core/Demo/Demo.h"
#include "Core/ImGuiSettings.h"
#include "Core/Info/Info.h"
#include "Core/Primitive/PrimitiveActionQueuer.h"
#include "Core/Windows.h"

#include "Audio/Audio.h"
#include "Project/ProjectSettings.h"
#include "Style/Style.h"

struct ProjectContext;

/**
`State` fully describes the FlowGrid application state at any point in time.
It's a structured representation of its underlying store (of type `Store`,
which is composed of an `immer::map<Path, {Type}>` for each stored type).
*/
struct ProjectState : Component, ActionableProducer<Action::State::Any, Action::Any> {
    ProjectState(Store &, ActionProducer::Enqueue, const ::ProjectContext &);
    ~ProjectState();

    const PrimitiveActionQueuer PrimitiveQ{CreateProducer<PrimitiveActionQueuer::ActionType>()};

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

    ProducerProp(fg::Style, Style);
    ProducerProp(Windows, Windows);
    Prop(ImGuiSettings, ImGuiSettings);
    Prop(ProjectSettings, Settings);
    Prop(Info, Info);
    ProducerProp(Demo, Demo);
    Prop(Debug, Debug, WindowFlags_NoScrollWithMouse);

    ProducerProp(Audio, Audio);

    void RenderDebug() const override;

protected:
    void Render() const override;
};
