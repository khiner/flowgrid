#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Primitive, Int,
    DefineComponentAction(Set, "", int value;);
    ComponentActionJson(Set, value);

    using Any = ActionVariant<Set>;
);
