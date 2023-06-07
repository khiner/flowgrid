#pragma once

#include "Core/Stateful/Field/Bool.h"
#include "Core/Stateful/Field/Enum.h"
#include "Core/Stateful/Window.h"

#include "nlohmann/json_fwd.hpp"

struct Metrics : TabsWindow {
    using TabsWindow::TabsWindow;

    DefineUI(FlowGridMetrics, Prop(Bool, ShowRelativePaths, true));
    DefineUI(ImGuiMetrics);
    DefineUI(ImPlotMetrics);

    Prop(FlowGridMetrics, FlowGrid);
    Prop(ImGuiMetrics, ImGui);
    Prop(ImPlotMetrics, ImPlot);
};

DefineUI(
    Debug,

    DefineWindow_(
        StateViewer,
        Menu({
            Menu("Settings", {AutoSelect, LabelMode}),
            Menu({}), // Need multiple elements to disambiguate vector-of-variants construction from variant construction.
        }),
        enum LabelMode{Annotated, Raw};
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
    );

    DefineWindow(
        ProjectPreview,
        Prop(Enum, Format, {"StateFormat", "ActionFormat"}, 1);
        Prop(Bool, Raw)
    );

    // DefineWindow_(StateMemoryEditor, WindowFlags_NoScrollbar);
    DefineWindow(StorePathUpdateFrequency);

    DefineWindow(DebugLog);
    DefineWindow(StackTool);

    Prop(StateViewer, StateViewer);
    Prop(ProjectPreview, ProjectPreview);
    // Prop(StateMemoryEditor, StateMemoryEditor);
    Prop(StorePathUpdateFrequency, StorePathUpdateFrequency);
    Prop(DebugLog, DebugLog);
    Prop(StackTool, StackTool);
    Prop(Metrics, Metrics);
);
