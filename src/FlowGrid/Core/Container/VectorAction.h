#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Primitive/PrimitiveJson.h"

namespace Action {
template<IsPrimitive T> struct Vector {
    static_assert(always_false_v<T>, "There is no `Vector` action type for this primitive type.");
};

DefineTemplatedActionType(
    Vector, Bool, bool,
    DefineFieldAction(SetAt, "", Count i; bool value;);

    using Any = ActionVariant<SetAt>;
);

DefineTemplatedActionType(
    Vector, Int, int,
    DefineFieldAction(SetAt, "", Count i; int value;);

    using Any = ActionVariant<SetAt>;
);

DefineTemplatedActionType(
    Vector, UInt, U32,
    DefineFieldAction(SetAt, "", Count i; U32 value;);

    using Any = ActionVariant<SetAt>;
);

DefineTemplatedActionType(
    Vector, Float, float,
    DefineFieldAction(SetAt, "", Count i; float value;);

    using Any = ActionVariant<SetAt>;
);

Json(Vector<bool>::SetAt, path, i, value);
Json(Vector<int>::SetAt, path, i, value);
Json(Vector<U32>::SetAt, path, i, value);
Json(Vector<float>::SetAt, path, i, value);
} // namespace Action
