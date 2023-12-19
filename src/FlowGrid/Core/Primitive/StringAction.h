#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Primitive, String,
    DefineComponentAction(Set, "", std::string value;);
    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);
