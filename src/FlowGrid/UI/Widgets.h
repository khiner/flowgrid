#pragma once

#include "nlohmann/json_fwd.hpp"

#include "Style.h"

using namespace nlohmann;

struct ImVec2;

namespace FlowGrid {}
namespace fg = FlowGrid;

enum InteractionFlags_ {
    InteractionFlags_None = 0,
    InteractionFlags_Hovered = 1 << 0,
    InteractionFlags_Held = 1 << 1,
    InteractionFlags_Clicked = 1 << 2,
};
using InteractionFlags = int;

namespace FlowGrid {
// Similar to `imgui_demo.cpp`'s `HelpMarker`.
void HelpMarker(const char *help);
// Basically `ImGui::InvisibleButton`, but supports hover/held testing.
InteractionFlags InvisibleButton(const ImVec2 &size_arg, const char *id);

enum JsonTreeNodeFlags_ {
    JsonTreeNodeFlags_None = 0,
    JsonTreeNodeFlags_Highlighted = 1 << 0,
    JsonTreeNodeFlags_Disabled = 1 << 1,
    JsonTreeNodeFlags_DefaultOpen = 1 << 2,
};
using JsonTreeNodeFlags = int;

bool JsonTreeNode(std::string_view label, const ImU32 highight_color, JsonTreeNodeFlags flags = JsonTreeNodeFlags_None, const char *id = nullptr);

// If `label` is empty, `JsonTree` will simply show the provided json `value` (object/array/raw value), with no nesting.
// For a non-empty `label`:
//   * If the provided `value` is an array or object, it will show as a nested `JsonTreeNode` with `label` as its parent.
//   * If the provided `value` is a raw value (or null), it will show as as '{label}: {value}'.
void JsonTree(std::string_view label, const json &value, const ImU32 highight_color, JsonTreeNodeFlags node_flags = JsonTreeNodeFlags_None, const char *id = nullptr);
} // namespace FlowGrid

enum KnobFlags_ {
    KnobFlags_None = 0,
    KnobFlags_NoTitle = 1 << 0,
    KnobFlags_NoInput = 1 << 1,
    KnobFlags_ValueTooltip = 1 << 2,
    KnobFlags_DragHorizontal = 1 << 3,
};
typedef int KnobFlags;

enum KnobVariant_ {
    KnobVariant_Tick = 1 << 0,
    KnobVariant_Dot = 1 << 1,
    KnobVariant_Wiper = 1 << 2,
    KnobVariant_WiperOnly = 1 << 3,
    KnobVariant_WiperDot = 1 << 4,
    KnobVariant_Stepped = 1 << 5,
    KnobVariant_Space = 1 << 6,
};
typedef int KnobVariant;

struct ColorSet {
    ColorSet(const U32 base, const U32 hovered, const U32 active) : base(base), hovered(hovered), active(active) {}
    ColorSet(const U32 color) : ColorSet(color, color, color) {}

    U32 base, hovered, active;
};

bool Knob(const char *label, float *p_value, float v_min, float v_max, float speed = 0, const char *format = nullptr, HJustify h_justify = HJustify_Middle, KnobVariant variant = KnobVariant_Tick, KnobFlags flags = KnobFlags_None, int steps = 10);
bool KnobInt(const char *label, int *p_value, int v_min, int v_max, float speed = 0, const char *format = nullptr, HJustify h_justify = HJustify_Middle, KnobVariant variant = KnobVariant_Tick, KnobFlags flags = KnobFlags_None, int steps = 10);
