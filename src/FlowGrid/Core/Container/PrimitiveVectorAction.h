#pragma once

#include "Core/Action/DefineAction.h"

namespace Action {
template<typename T> struct PrimitiveVector {
    static_assert(always_false_v<T>, "There is no `PrimitiveVector` action type for this type.");
};

DefineTemplatedActionType(
    PrimitiveVector, Bool, bool,
    DefineComponentAction(Set, "", u32 i; bool value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    PrimitiveVector, Int, int,
    DefineComponentAction(Set, "", u32 i; int value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    PrimitiveVector, UInt, u32,
    DefineComponentAction(Set, "", u32 i; u32 value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    PrimitiveVector, Float, float,
    DefineComponentAction(Set, "", u32 i; float value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    PrimitiveVector, String, std::string,
    DefineComponentAction(Set, "", u32 i; std::string value;);

    using Any = ActionVariant<Set>;
);

ComponentActionJson(PrimitiveVector<bool>::Set, i, value);
ComponentActionJson(PrimitiveVector<int>::Set, i, value);
ComponentActionJson(PrimitiveVector<u32>::Set, i, value);
ComponentActionJson(PrimitiveVector<float>::Set, i, value);
ComponentActionJson(PrimitiveVector<std::string>::Set, i, value);
} // namespace Action
