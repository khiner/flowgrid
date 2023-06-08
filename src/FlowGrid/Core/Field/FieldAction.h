#pragma once

#include "Core/Action/Action.h"
#include "Core/PathJson.h"
#include "Core/PrimitiveJson.h"

namespace Action {
Define(SetPrimitive, CustomMerge, "", StorePath path; Primitive value;);
Define(SetPrimitives, CustomMerge, "", std::vector<std::pair<StorePath, Primitive>> values;);
Define(ToggleBool, NoMerge, "", StorePath path;);

Json(SetPrimitive, path, value);
Json(SetPrimitives, values);
Json(ToggleBool, path);

using Primitive = ActionVariant<SetPrimitive, SetPrimitives, ToggleBool>;
} // namespace Action

namespace Action {
Define(SetVector, CustomMerge, "", StorePath path; std::vector<::Primitive> value;);

Json(SetVector, path, value);

using Vector = ActionVariant<SetVector>;
} // namespace Action

namespace Action {
Define(SetMatrix, CustomMerge, "", StorePath path; std::vector<::Primitive> data; Count row_count;);

Json(SetMatrix, path, data, row_count);

using Matrix = ActionVariant<SetMatrix>;
} // namespace Action
