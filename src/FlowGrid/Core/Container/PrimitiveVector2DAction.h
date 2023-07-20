#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Primitive/Primitive.h"

namespace Action {
template<IsPrimitive T> struct PrimitiveVector2D {
    static_assert(always_false_v<T>, "There is no `PrimitiveVector` action type for this primitive type.");
};

DefineTemplatedActionType(
    PrimitiveVector2D, Bool, bool,
    DefineFieldAction(Set, "", std::vector<std::vector<bool>> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    PrimitiveVector2D, Int, int,
    DefineFieldAction(Set, "", std::vector<std::vector<int>> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    PrimitiveVector2D, UInt, u32,
    DefineFieldAction(Set, "", std::vector<std::vector<u32>> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    PrimitiveVector2D, Float, float,
    DefineFieldAction(Set, "", std::vector<std::vector<float>> value;);

    using Any = ActionVariant<Set>;
);

Json(PrimitiveVector2D<bool>::Set, path, value);
Json(PrimitiveVector2D<int>::Set, path, value);
Json(PrimitiveVector2D<u32>::Set, path, value);
Json(PrimitiveVector2D<float>::Set, path, value);
} // namespace Action
