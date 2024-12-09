#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    String,
    DefineComponentAction(Set, Saved, SameIdMerge, "", std::string value;);
    ComponentActionJson(Set, value);

    using Any = ActionVariant<Set>;
);
