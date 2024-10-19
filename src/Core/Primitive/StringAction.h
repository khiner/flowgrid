#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Primitive, String,
    DefineComponentAction(Set, "", std::string value;);
    ComponentActionJson(Set, value);

    using Any = ActionVariant<Set>;
);
