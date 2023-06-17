#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/PrimitiveJson.h"

DefineActionType(
    Vector,
    DefineFieldAction(Set, "", std::vector<::Primitive> value;);

    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);
