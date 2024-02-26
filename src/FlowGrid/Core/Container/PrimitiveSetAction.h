#pragma once

#include "Core/Action/DefineAction.h"

namespace Action {
template<typename T> struct PrimitiveSet {
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
