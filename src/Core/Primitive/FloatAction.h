#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Primitive, Float,
    DefineComponentAction(Set, "", float value;);
    ComponentActionJson(Set, value);

    using Any = ActionVariant<Set>;
);
