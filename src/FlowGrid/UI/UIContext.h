#pragma once

#include "imgui.h"
#include "implot.h"
#include "imgui_internal.h"
#include "implot_internal.h"

#include "../Helper/Time.h"

// Adding/subtracting scalars to/from vectors is not defined with `IMGUI_DEFINE_MATH_OPERATORS`.
static inline ImVec2 operator+(const ImVec2 &lhs, const float rhs) { return {lhs.x + rhs, lhs.y + rhs}; }
static inline ImVec2 operator-(const ImVec2 &lhs, const float rhs) { return {lhs.x - rhs, lhs.y - rhs}; }
// Neither is multiplying by an `ImVec4`.
static inline ImVec4 operator*(const ImVec4 &lhs, const float rhs) { return {lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs}; }

struct UIContext {
    enum Flags_ {
        Flags_None = 0,
        Flags_ImGuiSettings = 1 << 0,
        Flags_ImGuiStyle = 1 << 1,
        Flags_ImPlotStyle = 1 << 2,
    };
    using Flags = int;

    struct Fonts {
        ImFont *Main{nullptr};
        ImFont *FixedWidth{nullptr};
    };

    void WidgetGestured() {
        if (ImGui::IsItemActivated()) IsWidgetGesturing = true;
        if (ImGui::IsItemDeactivated()) IsWidgetGesturing = false;
    }

    ImGuiContext *ImGui{nullptr};
    ImPlotContext *ImPlot{nullptr};
    Fonts Fonts{};

    bool IsWidgetGesturing{};
    Flags ApplyFlags = Flags_None;
};

extern UIContext UiContext; // Created in `main.cpp`
