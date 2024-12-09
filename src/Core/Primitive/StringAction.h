#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    String,
    DefineComponentAction(Set, "", std::string value;);
    ComponentActionJson(Set, value);

    using Any = ActionVariant<Set>;
);
