#pragma once

#include "Core/Action/DefineAction.h"

namespace Action {
template<typename T> struct PrimitiveVector2D {
    static_assert(always_false_v<T>, "There is no `PrimitiveVector` action type for this primitive type.");
};

DefineTemplatedActionType(
    PrimitiveVector2D, Bool, bool,
    DefineComponentAction(Set, "", std::vector<std::vector<bool>> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    PrimitiveVector2D, Int, int,
    DefineComponentAction(Set, "", std::vector<std::vector<int>> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    PrimitiveVector2D, UInt, u32,
    DefineComponentAction(Set, "", std::vector<std::vector<u32>> value;);

    using Any = ActionVariant<Set>;
);

DefineTemplatedActionType(
    PrimitiveVector2D, Float, float,
    DefineComponentAction(Set, "", std::vector<std::vector<float>> value;);

    using Any = ActionVariant<Set>;
);

ComponentActionJson(PrimitiveVector2D<bool>::Set, value);
ComponentActionJson(PrimitiveVector2D<int>::Set, value);
ComponentActionJson(PrimitiveVector2D<u32>::Set, value);
ComponentActionJson(PrimitiveVector2D<float>::Set, value);
} // namespace Action
