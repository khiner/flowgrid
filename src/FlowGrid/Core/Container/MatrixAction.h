#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Primitive/PrimitiveJson.h"

namespace Action {
template<IsPrimitive T> struct Matrix {
    static_assert(always_false_v<T>, "There is no `Matrix` action type for this primitive type.");
};

DefineTemplatedActionType(
    Matrix<bool>, Matrix / Bool,
    DefineFieldAction(Set, "", std::vector<bool> value; Count row_count;);
    DefineFieldAction(SetValue, "", Count row; Count col; bool value;);

    using Any = ActionVariant<Set, SetValue>;
);

Json(Matrix<bool>::Set, path, value, row_count);
Json(Matrix<bool>::SetValue, path, row, col, value);
} // namespace Action
