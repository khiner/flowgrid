#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Vec2,
    DefineComponentAction(Set, "", std::pair<float, float> value;);
    DefineComponentAction(SetX, "", float value;);
    DefineComponentAction(SetY, "", float value;);
    DefineComponentAction(SetAll, "", float value;);
    DefineComponentAction(ToggleLinked, ""); // No effect for non-linked `Vec2` fields.

    ComponentActionJson(Set, value);
    ComponentActionJson(SetX, value);
    ComponentActionJson(SetY, value);
    ComponentActionJson(SetAll, value);
    ComponentActionJson(ToggleLinked);

    using Any = ActionVariant<Set, SetX, SetY, SetAll, ToggleLinked>;
);
