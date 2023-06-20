#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Json.h"

DefineNestedActionType(
    Primitive, Flags,
    // todo toggle bit action instead of set
    DefineFieldAction(Set, "", int value;);
    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);
