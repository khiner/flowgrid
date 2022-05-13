#pragma once

#include "imgui_internal.h"
#include "implot_internal.h"

struct RenderContext;
struct UiContext {
    UiContext(ImGuiContext *imgui_context, ImPlotContext *implot_context) : imgui_context(imgui_context), implot_context(implot_context) {}

    ImGuiContext *imgui_context;
    ImPlotContext *implot_context;
};

UiContext create_ui();
void tick_ui(UiContext &);
void destroy_ui(UiContext &);
