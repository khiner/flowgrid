#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Vec2,
    DefineComponentAction(Set, "", std::pair<float, float> value;);
    DefineComponentAction(SetX, "", float value;);
    DefineComponentAction(SetY, "", float value;);
    DefineComponentAction(SetAll, "", float value;);
    DefineComponentAction(ToggleLinked, ""); // No effect for non-linked `Vec2` fields.

    Json(Set, path, value);
    Json(SetX, path, value);
    Json(SetY, path, value);
    Json(SetAll, path, value);
    Json(ToggleLinked, path);

    using Any = ActionVariant<Set, SetX, SetY, SetAll, ToggleLinked>;
);
