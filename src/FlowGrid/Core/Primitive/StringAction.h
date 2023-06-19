#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Json.h"

DefineActionType(
    Primitive::String,
    DefineFieldAction(Set, "", std::string value;);
    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);
