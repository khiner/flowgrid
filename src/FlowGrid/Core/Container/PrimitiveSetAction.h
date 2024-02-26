#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Primitive/PrimitiveVariant.h"

namespace Action {
template<IsPrimitive T> struct PrimitiveSet {
    static_assert(always_false_v<T>, "There is no `PrimitiveSet` action type for this primitive type.");
};

DefineTemplatedActionType(
    PrimitiveSet, UInt, u32,
    DefineComponentAction(Insert, "", u32 value;);
    DefineComponentAction(Erase, "", u32 value;);

    using Any = ActionVariant<Insert, Erase>;
);

ComponentActionJson(PrimitiveSet<u32>::Insert, value);
ComponentActionJson(PrimitiveSet<u32>::Erase, value);
} // namespace Action
