#pragma once

#include "Core/Action/DefineAction.h"

namespace Action {
template<typename T> struct Navigable {
    static_assert(always_false_v<T>, "There is no `Navigable` action type for this template type.");
};

DefineTemplatedActionType(
    Navigable, UInt, u32,
    DefineUnmergableComponentAction(Clear);
    DefineUnmergableComponentAction(Push, u32 value;);
    DefineComponentAction(MoveTo, "", u32 index;);

    using Any = ActionVariant<Clear, Push, MoveTo>;
);

ComponentActionJson(Navigable<u32>::Clear);
ComponentActionJson(Navigable<u32>::Push, value);
ComponentActionJson(Navigable<u32>::MoveTo, index);
} // namespace Action
