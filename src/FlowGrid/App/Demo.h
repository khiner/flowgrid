#pragma once

#include "Core/Window.h"
#include "FileDialog/FileDialog.h"

struct Demo : TabsWindow {
    Demo(ComponentArgs &&);

    DefineUI(ImGuiDemo);
    DefineUI(ImPlotDemo);

    Prop(ImGuiDemo, ImGui);
    Prop(ImPlotDemo, ImPlot);
    Prop(FileDialog::Demo, FileDialog);
};
