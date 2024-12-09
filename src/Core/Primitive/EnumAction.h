#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Enum,
    DefineComponentAction(Set, Saved, SameIdMerge, "", int value;);
    ComponentActionJson(Set, value);

    using Any = ActionVariant<Set>;
);
