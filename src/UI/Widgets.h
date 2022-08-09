#pragma once

#include "../State.h"

namespace FlowGrid {

// Helper to display a (?) mark which shows a tooltip when hovered. From `imgui_demo.cpp`.
void HelpMarker(const char *desc);

bool Checkbox(const JsonPath &path, const char *label = nullptr);
bool SliderFloat(const JsonPath &path, float v_min, float v_max, const char *format = "%.3f", ImGuiSliderFlags flags = 0, const char *label = nullptr);
bool SliderFloat2(const JsonPath &path, float v_min, float v_max, const char *format = "%.3f", ImGuiSliderFlags flags = 0);
bool SliderInt(const JsonPath &path, int v_min, int v_max, const char *format = "%d", ImGuiSliderFlags flags = 0, const char *label = nullptr);

bool DragFloat(const JsonPath &path, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char *format = "%.3f", ImGuiSliderFlags flags = 0, const char *label = nullptr);

bool ColorEdit4(const JsonPath &path, ImGuiColorEditFlags flags = 0, const char *label = nullptr);

using ActionID = size_t; // duplicate definition to avoid importing `Action.h`

// For actions with no data members.
void MenuItem(ActionID);

bool Combo(const JsonPath &path, const char *items_separated_by_zeros, int popup_max_height_in_items = -1);

typedef int JsonTreeNodeFlags;
enum JsonTreeNodeFlags_ {
    JsonTreeNodeFlags_None = 0,
    JsonTreeNodeFlags_Highlighted = 1 << 0,
    JsonTreeNodeFlags_Disabled = 1 << 1,
    JsonTreeNodeFlags_DefaultOpen = 1 << 2,
};

bool JsonTreeNode(const string &label, JsonTreeNodeFlags flags = JsonTreeNodeFlags_None, const char *id = nullptr);

// If `label` is empty, `JsonTree` will simply show the provided json `value` (object/array/raw value), with no nesting.
// For a non-empty `label`:
//   * If the provided `value` is an array or object, it will show as a nested `JsonTreeNode` with `label` as its parent.
//   * If the provided `value` is a raw value (or null), it will show as as '{label}: {value}'.
void JsonTree(const string &label, const json &value, JsonTreeNodeFlags node_flags = JsonTreeNodeFlags_None, const char *id = nullptr);

}
