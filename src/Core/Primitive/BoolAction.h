#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Bool,
    DefineComponentAction(Toggle, "");
    ComponentActionJson(Toggle);

    using Any = ActionVariant<Toggle>;
);
