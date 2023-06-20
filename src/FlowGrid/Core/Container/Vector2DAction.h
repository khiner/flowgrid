#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Primitive/PrimitiveJson.h"

namespace Action {
template<IsPrimitive T> struct Vector2D {
    static_assert(always_false_v<T>, "There is no `Vector` action type for this primitive type.");
};

DefineTemplatedActionType(
    Vector2D<bool>, Vector2D / Bool,
    DefineFieldAction(Set, "", std::vector<std::vector<bool>> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    Vector2D<int>, Vector2D / Int,
    DefineFieldAction(Set, "", std::vector<std::vector<int>> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    Vector2D<U32>, Vector2D / UInt,
    DefineFieldAction(Set, "", std::vector<std::vector<U32>> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    Vector2D<float>, Vector2D / Float,
    DefineFieldAction(Set, "", std::vector<std::vector<float>> value;);

    using Any = ActionVariant<Set>;
);

Json(Vector2D<bool>::Set, path, value);
Json(Vector2D<int>::Set, path, value);
Json(Vector2D<U32>::Set, path, value);
Json(Vector2D<float>::Set, path, value);
} // namespace Action
