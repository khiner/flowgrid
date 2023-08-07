#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Primitive, Int,
    DefineFieldAction(Set, "", int value;);
    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);
