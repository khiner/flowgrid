#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/PrimitiveJson.h"

DefineActionType(
    Vec2,
    DefineFieldAction(Set, "", std::pair<float, float> value;);
    DefineFieldAction(SetAll, "", float value;);

    Json(Set, path, value);
    Json(SetAll, path, value);

    using Any = ActionVariant<Set, SetAll>;
);
