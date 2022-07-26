#pragma once

#include "imgui.h"
#include "implot.h"
#include "imgui_internal.h"
#include "implot_internal.h"

struct UIContext {
    UIContext(ImGuiContext *imgui_context, ImPlotContext *implot_context) : imgui_context(imgui_context), implot_context(implot_context) {}

    ImGuiContext *imgui_context;
    ImPlotContext *implot_context;
};
