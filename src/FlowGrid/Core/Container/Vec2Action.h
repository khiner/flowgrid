#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Vec2,
    DefineFieldAction(Set, "", std::pair<float, float> value;);
    DefineFieldAction(SetX, "", float value;);
    DefineFieldAction(SetY, "", float value;);
    DefineFieldAction(SetAll, "", float value;);
    DefineFieldAction(ToggleLinked, ""); // No effect for non-linked `Vec2` fields.

    Json(Set, path, value);
    Json(SetX, path, value);
    Json(SetY, path, value);
    Json(SetAll, path, value);
    Json(ToggleLinked, path);

    using Any = ActionVariant<Set, SetX, SetY, SetAll, ToggleLinked>;
);
