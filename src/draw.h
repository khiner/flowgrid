#pragma once

#include <SDL.h>
#include <SDL_opengl.h>

#include <utility>

#include "imgui_internal.h"
#include "implot_internal.h"
#include "imgui_impl_sdl.h" // TODO should still be able to get `RenderContext` into draw.cpp and remove all SDL `include`s from the header

struct RenderContext {
    SDL_Window *window = nullptr;
    SDL_GLContext gl_context{};
    const char *glsl_version = "#version 150";
    ImGuiIO io;
};

struct UiContext {
    UiContext(RenderContext render_context,
              ImGuiContext *imgui_context,
              ImPlotContext *implot_context)
        : render_context(std::move(render_context)), imgui_context(imgui_context), implot_context(implot_context) {}

    RenderContext render_context;
    ImGuiContext *imgui_context;
    ImPlotContext *implot_context;
};

UiContext create_ui();
void tick_ui(UiContext &);
void destroy_ui(UiContext &);
