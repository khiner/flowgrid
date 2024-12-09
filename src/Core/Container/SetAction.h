#pragma once

#include "Core/Action/DefineAction.h"

namespace Action {
template<typename T> struct Set {
    static_assert(always_false_v<T>, "There is no `Set` action type for this type.");
};

DefineTemplatedActionType(
    Set, UInt, u32,
    DefineComponentAction(Insert, Saved, SameIdMerge, "", u32 value;);
    DefineComponentAction(Erase, Saved, SameIdMerge, "", u32 value;);

    using Any = ActionVariant<Insert, Erase>;
);

ComponentActionJson(Set<u32>::Insert, value);
ComponentActionJson(Set<u32>::Erase, value);
} // namespace Action
