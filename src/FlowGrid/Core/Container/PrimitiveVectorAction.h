#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Primitive/Primitive.h"

namespace Action {
template<IsPrimitive T> struct PrimitiveVector {
    static_assert(always_false_v<T>, "There is no `PrimitiveVector` action type for this primitive type.");
};

DefineTemplatedActionType(
    PrimitiveVector, Bool, bool,
    DefineComponentAction(SetAt, "", u32 i; bool value;);

    using Any = ActionVariant<SetAt>;
);

DefineTemplatedActionType(
    PrimitiveVector, Int, int,
    DefineComponentAction(SetAt, "", u32 i; int value;);

    using Any = ActionVariant<SetAt>;
);

DefineTemplatedActionType(
    PrimitiveVector, UInt, u32,
    DefineComponentAction(SetAt, "", u32 i; u32 value;);

    using Any = ActionVariant<SetAt>;
);

DefineTemplatedActionType(
    PrimitiveVector, Float, float,
    DefineComponentAction(SetAt, "", u32 i; float value;);

    using Any = ActionVariant<SetAt>;
);

DefineTemplatedActionType(
    PrimitiveVector, String, std::string,
    DefineComponentAction(SetAt, "", u32 i; std::string value;);

    using Any = ActionVariant<SetAt>;
);

Json(PrimitiveVector<bool>::SetAt, path, i, value);
Json(PrimitiveVector<int>::SetAt, path, i, value);
Json(PrimitiveVector<u32>::SetAt, path, i, value);
Json(PrimitiveVector<float>::SetAt, path, i, value);
Json(PrimitiveVector<std::string>::SetAt, path, i, value);
} // namespace Action
