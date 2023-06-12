#pragma once

#include "Core/Window.h"
#include "FileDialog/FileDialog.h"

struct Demo : TabsWindow {
    Demo(Component *parent, string_view path_leaf, string_view meta_str);

    DefineUI(ImGuiDemo);
    DefineUI(ImPlotDemo);

    Prop(ImGuiDemo, ImGui);
    Prop(ImPlotDemo, ImPlot);
    Prop(FileDialog::Demo, FileDialog);
};
