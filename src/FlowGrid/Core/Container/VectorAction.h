#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Primitive/PrimitiveJson.h"

namespace Action {
template<IsPrimitive T> struct Vector {
    static_assert(always_false_v<T>, "There is no `Vector` action type for this primitive type.");
};

DefineTemplatedActionType(
    Vector, Bool, bool,
    DefineFieldAction(Set, "", std::vector<bool> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    Vector, Int, int,
    DefineFieldAction(Set, "", std::vector<int> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    Vector, UInt, U32,
    DefineFieldAction(Set, "", std::vector<U32> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    Vector, Float, float,
    DefineFieldAction(Set, "", std::vector<float> value;);

    using Any = ActionVariant<Set>;
);

Json(Vector<bool>::Set, path, value);
Json(Vector<int>::Set, path, value);
Json(Vector<U32>::Set, path, value);
Json(Vector<float>::Set, path, value);
} // namespace Action
