#pragma once

#include "Core/Action/Actionable.h"
#include "StoreTypesJson.h"

namespace Action {
using namespace Actionable;
Define(ToggleValue, 1, 0, NoMerge, "", StorePath path;);
Define(SetValue, 1, 0, CustomMerge, "", StorePath path; Primitive value;);
Define(SetValues, 1, 0, CustomMerge, "", StoreEntries values;);
Define(SetVector, 1, 0, CustomMerge, "", StorePath path; std::vector<Primitive> value;);
Define(SetMatrix, 1, 0, CustomMerge, "", StorePath path; std::vector<Primitive> data; Count row_count;);
Define(ApplyPatch, 1, 0, CustomMerge, "", Patch patch;);

Json(SetValue, path, value);
Json(SetValues, values);
Json(SetVector, path, value);
Json(SetMatrix, path, data, row_count);
Json(ToggleValue, path);
Json(ApplyPatch, patch);

using StoreAction = ActionVariant<SetValue, SetValues, SetVector, SetMatrix, ToggleValue, ApplyPatch>;
} // namespace Action
