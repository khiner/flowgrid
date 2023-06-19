#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Json.h"

DefineActionType(
    Primitive::Float,
    DefineFieldAction(Set, "", float value;);
    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);
