#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/PrimitiveJson.h"

DefineActionType(
    Vec2,
    DefineAction(Set, CustomMerge, "", StorePath path; std::pair<float, float> value;);
    DefineAction(SetAll, CustomMerge, "", StorePath path; float value;);

    Json(Set, path, value);
    Json(SetAll, path, value);

    using Any = ActionVariant<Set, SetAll>;
);
