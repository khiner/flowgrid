#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/PrimitiveJson.h"

DefineActionType(
    Matrix,
    DefineFieldAction(Set, "", std::vector<::Primitive> value; Count row_count;);

    Json(Set, path, value, row_count);

    using Any = ActionVariant<Set>;
);
