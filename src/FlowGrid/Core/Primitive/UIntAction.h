#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Primitive, UInt,
    DefineFieldAction(Set, "", unsigned int value;);
    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);
