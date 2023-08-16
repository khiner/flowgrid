#pragma once

#include "Core/Action/DefineAction.h"

namespace Action {
template<typename T> struct Navigable {
    static_assert(always_false_v<T>, "There is no `Navigable` action type for this template type.");
};

DefineTemplatedActionType(
    Navigable, UInt, u32,
    DefineUnmergableFieldAction(Push, u32 value;);
    DefineFieldAction(MoveTo, "", u32 index;);

    using Any = ActionVariant<Push, MoveTo>;
);

Json(Navigable<u32>::Push, path, value);
Json(Navigable<u32>::MoveTo, path, index);
} // namespace Action
