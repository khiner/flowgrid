#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Int,
    DefineComponentAction(Set, "", int value;);
    ComponentActionJson(Set, value);

    using Any = ActionVariant<Set>;
);
