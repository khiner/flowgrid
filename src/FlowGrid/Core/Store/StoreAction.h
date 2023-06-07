#pragma once

#include "Core/Action/Action.h"
#include "StoreTypesJson.h"

namespace Action {
Define(ToggleValue, 1, NoMerge, "", StorePath path;);
Define(SetValue, 1, CustomMerge, "", StorePath path; Primitive value;);
Define(SetValues, 1, CustomMerge, "", StoreEntries values;);
Define(SetVector, 1, CustomMerge, "", StorePath path; std::vector<Primitive> value;);
Define(SetMatrix, 1, CustomMerge, "", StorePath path; std::vector<Primitive> data; Count row_count;);
Define(ApplyPatch, 1, CustomMerge, "", Patch patch;);

Json(SetValue, path, value);
Json(SetValues, values);
Json(SetVector, path, value);
Json(SetMatrix, path, data, row_count);
Json(ToggleValue, path);
Json(ApplyPatch, patch);

using Store = ActionVariant<SetValue, SetValues, SetVector, SetMatrix, ToggleValue, ApplyPatch>;
} // namespace Action
