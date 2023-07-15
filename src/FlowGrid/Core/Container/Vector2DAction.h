#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Primitive/PrimitiveJson.h"

namespace Action {
template<IsPrimitive T> struct Vector2D {
    static_assert(always_false_v<T>, "There is no `Vector` action type for this primitive type.");
};

DefineTemplatedActionType(
    Vector2D, Bool, bool,
    DefineFieldAction(Set, "", std::vector<std::vector<bool>> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    Vector2D, Int, int,
    DefineFieldAction(Set, "", std::vector<std::vector<int>> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    Vector2D, UInt, u32,
    DefineFieldAction(Set, "", std::vector<std::vector<u32>> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    Vector2D, Float, float,
    DefineFieldAction(Set, "", std::vector<std::vector<float>> value;);

    using Any = ActionVariant<Set>;
);

Json(Vector2D<bool>::Set, path, value);
Json(Vector2D<int>::Set, path, value);
Json(Vector2D<u32>::Set, path, value);
Json(Vector2D<float>::Set, path, value);
} // namespace Action
