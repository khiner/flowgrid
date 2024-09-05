#pragma once

#include "Core/Action/DefineAction.h"

namespace Action {
template<typename T> struct Vector {
    static_assert(always_false_v<T>, "There is no `Vector` action type for this type.");
};

DefineTemplatedActionType(
    Vector, Bool, bool,
    DefineComponentAction(Set, "", u32 i; bool value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    Vector, Int, int,
    DefineComponentAction(Set, "", u32 i; int value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    Vector, UInt, u32,
    DefineComponentAction(Set, "", u32 i; u32 value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    Vector, Float, float,
    DefineComponentAction(Set, "", u32 i; float value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    Vector, String, std::string,
    DefineComponentAction(Set, "", u32 i; std::string value;);

    using Any = ActionVariant<Set>;
);

ComponentActionJson(Vector<bool>::Set, i, value);
ComponentActionJson(Vector<int>::Set, i, value);
ComponentActionJson(Vector<u32>::Set, i, value);
ComponentActionJson(Vector<float>::Set, i, value);
ComponentActionJson(Vector<std::string>::Set, i, value);
} // namespace Action
