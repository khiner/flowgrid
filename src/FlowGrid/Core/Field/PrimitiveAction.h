#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/PrimitiveJson.h"

DefineActionType(
    Primitive::Bool,
    DefineAction(Toggle, NoMerge, "", StorePath path;);

    Json(Toggle, path);

    using Any = ActionVariant<Toggle>;
);

DefineActionType(
    Primitive,
    DefineFieldAction(Set, "", ::Primitive value;);

    Json(Set, path, value);

    using Any = ActionVariant<Set, Bool::Toggle>;
);
