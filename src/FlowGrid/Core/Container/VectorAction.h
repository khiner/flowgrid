#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Primitive/PrimitiveJson.h"

namespace Action {
template<IsPrimitive T> struct Vector {
    static_assert(always_false_v<T>, "There is no `Vector` action type for this primitive type.");
};

DefineTemplatedActionType(
    Vector, Bool, bool,
    DefineFieldAction(SetAt, "", u32 i; bool value;);

    using Any = ActionVariant<SetAt>;
);

DefineTemplatedActionType(
    Vector, Int, int,
    DefineFieldAction(SetAt, "", u32 i; int value;);

    using Any = ActionVariant<SetAt>;
);

DefineTemplatedActionType(
    Vector, UInt, u32,
    DefineFieldAction(SetAt, "", u32 i; u32 value;);

    using Any = ActionVariant<SetAt>;
);

DefineTemplatedActionType(
    Vector, Float, float,
    DefineFieldAction(SetAt, "", u32 i; float value;);

    using Any = ActionVariant<SetAt>;
);

Json(Vector<bool>::SetAt, path, i, value);
Json(Vector<int>::SetAt, path, i, value);
Json(Vector<u32>::SetAt, path, i, value);
Json(Vector<float>::SetAt, path, i, value);
} // namespace Action
