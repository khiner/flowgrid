#pragma once

#include "imgui.h"
#include "implot.h"

#include "../Helper/Time.h"

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

UIContext CreateUi();
void TickUi();
void DestroyUi();

extern UIContext UiContext; // Created in `main.cpp`

//-----------------------------------------------------------------------------
// [SECTION] Widgets
//-----------------------------------------------------------------------------

// Adding/subtracting scalars to/from vectors is not defined with `IMGUI_DEFINE_MATH_OPERATORS`.
static constexpr ImVec2 operator+(const ImVec2 &lhs, const float rhs) { return {lhs.x + rhs, lhs.y + rhs}; }
static constexpr ImVec2 operator-(const ImVec2 &lhs, const float rhs) { return {lhs.x - rhs, lhs.y - rhs}; }
// Neither is multiplying by an `ImVec4`.
static constexpr ImVec4 operator*(const ImVec4 &lhs, const float rhs) { return {lhs.x * rhs, lhs.y * rhs, lhs.z * rhs, lhs.w * rhs}; }

namespace FlowGrid {
using namespace nlohmann;
void HelpMarker(const char *help); // Like the one defined in `imgui_demo.cpp`

enum InteractionFlags_ {
    InteractionFlags_None = 0,
    InteractionFlags_Hovered = 1 << 0,
    InteractionFlags_Held = 1 << 1,
    InteractionFlags_Clicked = 1 << 2,
};
using InteractionFlags = int;

InteractionFlags InvisibleButton(const ImVec2 &size_arg, const char *id); // Basically `ImGui::InvisibleButton`, but supporting hover/held testing.

enum JsonTreeNodeFlags_ {
    JsonTreeNodeFlags_None = 0,
    JsonTreeNodeFlags_Highlighted = 1 << 0,
    JsonTreeNodeFlags_Disabled = 1 << 1,
    JsonTreeNodeFlags_DefaultOpen = 1 << 2,
};
using JsonTreeNodeFlags = int;

bool JsonTreeNode(string_view label, JsonTreeNodeFlags flags = JsonTreeNodeFlags_None, const char *id = nullptr);

// If `label` is empty, `JsonTree` will simply show the provided json `value` (object/array/raw value), with no nesting.
// For a non-empty `label`:
//   * If the provided `value` is an array or object, it will show as a nested `JsonTreeNode` with `label` as its parent.
//   * If the provided `value` is a raw value (or null), it will show as as '{label}: {value}'.
void JsonTree(string_view label, const json &value, JsonTreeNodeFlags node_flags = JsonTreeNodeFlags_None, const char *id = nullptr);
} // namespace FlowGrid
