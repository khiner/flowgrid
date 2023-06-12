#pragma once

#include "Core/Stateful/Window.h"
#include "FileDialog/FileDialog.h"

struct Demo : TabsWindow {
    Demo(Stateful *parent, string_view path_leaf, string_view meta_str);

    DefineUI(ImGuiDemo);
    DefineUI(ImPlotDemo);

    Prop(ImGuiDemo, ImGui);
    Prop(ImPlotDemo, ImPlot);
    Prop(FileDialog::Demo, FileDialog);
};
