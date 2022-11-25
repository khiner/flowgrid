#pragma once

#include "string"

#include "imgui.h"
#include "nlohmann/json_fwd.hpp"

using std::string;

namespace FlowGrid {
void HelpMarker(const char *help); // Like the one defined in `imgui_demo.cpp`
bool InvisibleButton(const ImVec2 &size_arg, bool *out_hovered, bool *out_held); // Basically `ImGui::InvisibleButton`, but supporting hover/held testing.

enum JsonTreeNodeFlags_ {
    JsonTreeNodeFlags_None = 0,
    JsonTreeNodeFlags_Highlighted = 1 << 0,
    JsonTreeNodeFlags_Disabled = 1 << 1,
    JsonTreeNodeFlags_DefaultOpen = 1 << 2,
};
using JsonTreeNodeFlags = int;

bool JsonTreeNode(const string &label, JsonTreeNodeFlags flags = JsonTreeNodeFlags_None, const char *id = nullptr);

// If `label` is empty, `JsonTree` will simply show the provided json `value` (object/array/raw value), with no nesting.
// For a non-empty `label`:
//   * If the provided `value` is an array or object, it will show as a nested `JsonTreeNode` with `label` as its parent.
//   * If the provided `value` is a raw value (or null), it will show as as '{label}: {value}'.
void JsonTree(const string &label, const json &value, JsonTreeNodeFlags node_flags = JsonTreeNodeFlags_None, const char *id = nullptr);
} // End `FlowGrid` namespace
