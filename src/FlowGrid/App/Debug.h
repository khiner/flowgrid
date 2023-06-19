#pragma once

#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Enum.h"

#include "nlohmann/json_fwd.hpp"

struct Metrics : Component, Drawable {
    using Component::Component;

    struct FlowGridMetrics : Component, Drawable {
        using Component::Component;
        Prop(Bool, ShowRelativePaths, true);

    protected:
        void Render() const override;
    };

    struct ImGuiMetrics : Component, Drawable {
        using Component::Component;

    protected:
        void Render() const override;
    };

    struct ImPlotMetrics : Component, Drawable {
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

struct Debug : Component, Drawable {
    using Component::Component;

    struct StateViewer : Component, Drawable {
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
        Prop_(Enum, LabelMode, "?The raw dog JSON state doesn't store keys for all items.\n"
                               "For example, the main `ui.style.colors` state is a list.\n\n"
                               "'Annotated' mode shows (highlighted) labels for such state items.\n"
                               "'Raw' mode shows the state exactly as it is in the raw JSON state.",
              {"Annotated", "Raw"}, Annotated);
        Prop_(Bool, AutoSelect, "Auto-Select?When auto-select is enabled, state changes automatically open.\n"
                                "The state viewer to the changed state node(s), closing all other state nodes.\n"
                                "State menu items can only be opened or closed manually if auto-select is disabled.",
              true);

        void StateJsonTree(string_view key, const nlohmann::json &value, const StorePath &path = RootPath) const;

    protected:
        void Render() const override;
    };

    struct ProjectPreview : Component, Drawable {
        using Component::Component;

        Prop(Enum, Format, {"StateFormat", "ActionFormat"}, 1);
        Prop(Bool, Raw);

    protected:
        void Render() const override;
    };

    // StateMemoryEditor, WindowFlags_NoScrollbar
    struct StorePathUpdateFrequency : Component, Drawable {
        using Component::Component;

    protected:
        void Render() const override;
    };

    struct DebugLog : Component, Drawable {
        using Component::Component;

    protected:
        void Render() const override;
    };

    struct StackTool : Component, Drawable {
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
