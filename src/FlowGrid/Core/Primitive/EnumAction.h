#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Primitive, Enum,
    DefineComponentAction(Set, "", int value;);
    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);
