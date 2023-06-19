#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Json.h"

DefineActionType(
    Primitive::Int,
    DefineFieldAction(Set, "", int value;);
    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);
