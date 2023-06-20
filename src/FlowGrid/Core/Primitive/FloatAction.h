#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Json.h"

DefineNestedActionType(
    Primitive, Float,
    DefineFieldAction(Set, "", float value;);
    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);
