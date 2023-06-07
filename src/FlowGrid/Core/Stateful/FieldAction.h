#pragma once

#include "Core/Action/Action.h"
#include "Core/Store/StoreEntries.h"

#include "Core/Store/PatchAction.h" // xxx only needed for path json conversion, which should be moved out.

namespace Action {
Define(SetValue, 1, CustomMerge, "", StorePath path; Primitive value;);
Define(ToggleValue, 1, NoMerge, "", StorePath path;);

Json(SetValue, path, value);
Json(ToggleValue, path);

using Value = ActionVariant<SetValue, ToggleValue>;
} // namespace Action

namespace Action {
Define(SetValues, 1, CustomMerge, "", StoreEntries values;);

Json(SetValues, values);

using Values = ActionVariant<SetValues>;
} // namespace Action

namespace Action {
Define(SetVector, 1, CustomMerge, "", StorePath path; std::vector<Primitive> value;);

Json(SetVector, path, value);

using Vector = ActionVariant<SetVector>;
} // namespace Action

namespace Action {
Define(SetMatrix, 1, CustomMerge, "", StorePath path; std::vector<Primitive> data; Count row_count;);

Json(SetMatrix, path, data, row_count);

using Matrix = ActionVariant<SetMatrix>;
} // namespace Action
