#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Primitive/PrimitiveJson.h"

DefineActionType(
    Matrix,
    DefineFieldAction(Set, "", std::vector<::Primitive> value; Count row_count;);
    DefineFieldAction(SetValue, "", Count row; Count col; ::Primitive value;);

    Json(Set, path, value, row_count);
    Json(SetValue, path, row, col, value);

    using Any = ActionVariant<Set, SetValue>;
);
