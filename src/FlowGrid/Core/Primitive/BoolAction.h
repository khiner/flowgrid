#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/Json.h"

DefineActionType(
    Primitive::Bool,
    DefineFieldAction(Toggle, "");
    Json(Toggle, path);

    using Any = ActionVariant<Toggle>;
);
