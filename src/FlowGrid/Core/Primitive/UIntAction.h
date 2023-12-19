#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Primitive, UInt,
    DefineComponentAction(Set, "", unsigned int value;);
    ComponentActionJson(Set, value);

    using Any = ActionVariant<Set>;
);
