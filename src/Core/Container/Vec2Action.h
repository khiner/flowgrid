#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Vec2,
    DefineComponentAction(Set, Saved, SameIdMerge, "", std::pair<float, float> value;);
    DefineComponentAction(SetX, Saved, SameIdMerge, "", float value;);
    DefineComponentAction(SetY, Saved, SameIdMerge, "", float value;);
    DefineComponentAction(SetAll, Saved, SameIdMerge, "", float value;);
    DefineComponentAction(ToggleLinked, Saved, SameIdMerge, ""); // No effect for non-linked `Vec2` fields.

    ComponentActionJson(Set, value);
    ComponentActionJson(SetX, value);
    ComponentActionJson(SetY, value);
    ComponentActionJson(SetAll, value);
    ComponentActionJson(ToggleLinked);

    using Any = ActionVariant<Set, SetX, SetY, SetAll, ToggleLinked>;
);
