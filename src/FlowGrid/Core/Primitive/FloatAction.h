#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Primitive, Float,
    DefineComponentAction(Set, "", float value;);
    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);
