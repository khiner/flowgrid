#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/PrimitiveJson.h"

DefineActionType(
    Vector,
    DefineAction(Set, CustomMerge, "", StorePath path; std::vector<::Primitive> value;);

    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);
