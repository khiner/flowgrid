#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Primitive, Bool,
    DefineComponentAction(Toggle, "");
    ComponentActionJson(Toggle);

    using Any = ActionVariant<Toggle>;
);
