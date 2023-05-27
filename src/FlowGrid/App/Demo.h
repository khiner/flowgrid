#pragma once

#include "Core/Stateful/WindowMember.h"
#include "FileDialog/FileDialog.h"

struct Demo : TabsWindow {
    Demo(StateMember *parent, string_view path_segment, string_view name_help);

    UIMember(ImGuiDemo);
    UIMember(ImPlotDemo);

    Prop(ImGuiDemo, ImGui);
    Prop(ImPlotDemo, ImPlot);
    Prop(FileDialog::Demo, FileDialog);
};
