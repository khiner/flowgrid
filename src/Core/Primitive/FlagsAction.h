#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Flags,
    // todo toggle bit action instead of set
    DefineComponentAction(Set, "", int value;);
    ComponentActionJson(Set, value);

    using Any = ActionVariant<Set>;
);
