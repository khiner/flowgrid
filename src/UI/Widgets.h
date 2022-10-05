#pragma once

#include <string>
#include "../JsonType.h"

struct StateMember;
using ActionID = size_t;
using ImGuiColorEditFlags = int;
using std::string;

namespace FlowGrid {

void gestured();

bool ColorEdit4(const JsonPath &path, ImGuiColorEditFlags flags = 0, const char *label = nullptr);

void MenuItem(ActionID); // For actions with no data members.
void ToggleMenuItem(const StateMember &);

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

}
