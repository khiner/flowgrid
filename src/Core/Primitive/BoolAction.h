#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Bool,
    DefineComponentAction(Toggle, Saved, SameIdMerge, "");
    ComponentActionJson(Toggle);

    using Any = ActionVariant<Toggle>;
);
