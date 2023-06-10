#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/PrimitiveJson.h"

DefineActionType(
    Matrix,
    DefineAction(Set, CustomMerge, "", StorePath path; std::vector<::Primitive> data; Count row_count;);

    Json(Set, path, data, row_count);

    using Any = ActionVariant<Set>;
);
