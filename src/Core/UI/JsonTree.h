#pragma once

#include <string_view>

#include "nlohmann/json_fwd.hpp"

using json = nlohmann::json;

namespace flowgrid {
// If `label` is empty, `JsonTree` will simply show the provided json `value` (object/array/raw value), with no nesting.
// For a non-empty `label`:
//   * If the provided `value` is an array or object, it will show as a nested `TreeNode` with `label` as its parent.
//   * If the provided `value` is a raw value (or null), it will show as as '{label}: {value}'.
bool TreeNode(std::string_view label, const char *id = nullptr, const char *value = nullptr);
void JsonTree(std::string_view label, json &&value, const char *id = nullptr);
} // namespace flowgrid
