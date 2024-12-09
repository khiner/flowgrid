#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Float,
    DefineComponentAction(Set, Saved, SameIdMerge, "", float value;);
    ComponentActionJson(Set, value);

    using Any = ActionVariant<Set>;
);
