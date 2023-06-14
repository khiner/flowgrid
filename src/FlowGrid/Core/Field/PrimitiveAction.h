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
    DefineAction(Set, CustomMerge, "", StorePath path; ::Primitive value;);
    DefineAction(SetMany, CustomMerge, "", std::vector<std::pair<StorePath, ::Primitive>> values;);

    Json(Set, path, value);
    Json(SetMany, values);

    using Any = ActionVariant<Set, SetMany, Bool::Toggle>;
);
