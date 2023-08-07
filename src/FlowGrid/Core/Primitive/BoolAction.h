#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Primitive, Bool,
    DefineFieldAction(Toggle, "");
    Json(Toggle, path);

    using Any = ActionVariant<Toggle>;
);
