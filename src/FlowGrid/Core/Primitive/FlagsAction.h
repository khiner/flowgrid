#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Primitive, Flags,
    // todo toggle bit action instead of set
    DefineComponentAction(Set, "", int value;);
    ComponentActionJson(Set,  value);

    using Any = ActionVariant<Set>;
);
