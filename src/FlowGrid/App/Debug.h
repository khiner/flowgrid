#pragma once

#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Enum.h"

#include "nlohmann/json_fwd.hpp"

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

struct Debug : Component {
    using Component::Component;

    struct StateViewer : Component {
        StateViewer(ComponentArgs &&args)
            : Component(
                  std::move(args),
                  Menu({
                      Menu("Settings", {AutoSelect, LabelMode}),
                      Menu({}), // Need multiple elements to disambiguate vector-of-variants construction from variant construction.
                  })
              ) {}

        enum LabelMode {
            Annotated,
            Raw
        };
        Prop_(Enum, LabelMode, "?'Raw' mode shows plain data structures and 'Annotated' mode shows (highlighted) human-readable labels in some cases.\n"
                               "For example, colors are stored as lists with a separate label mapping."
                               "When 'Annotated' mode is enabled, color keys are shown as labels instead of indexes.",
              {"Annotated", "Raw"}, Annotated);
        Prop_(Bool, AutoSelect, "Auto-Select?When enabled, changes to state automatically expand the tree to open the changed field value leaf, closing all other state nodes.\n"
                                "State menu items can only be opened or closed manually if auto-select is disabled.",
              true);

        void StateJsonTree(string_view key, const nlohmann::json &value, const StorePath &path = RootPath) const;

    protected:
        void Render() const override;
    };

    struct ProjectPreview : Component {
        using Component::Component;

        Prop(Enum, Format, {"StateFormat", "ActionFormat"}, 1);
        Prop(Bool, Raw);

    protected:
        void Render() const override;
    };

    // StateMemoryEditor, WindowFlags_NoScrollbar
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

    Prop(StateViewer, StateViewer);
    Prop(ProjectPreview, ProjectPreview);
    // Prop(StateMemoryEditor, StateMemoryEditor);
    Prop(StorePathUpdateFrequency, StorePathUpdateFrequency);
    Prop(DebugLog, DebugLog);
    Prop(StackTool, StackTool);
    Prop(Metrics, Metrics);

protected:
    void Render() const override;
};
