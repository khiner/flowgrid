#pragma once

#include "Core/Action/DefineAction.h"
#include "Core/PrimitiveJson.h"

DefineActionType(
    Primitive,
    DefineAction(Set, CustomMerge, "", StorePath path; ::Primitive value;);
    DefineAction(SetMany, CustomMerge, "", std::vector<std::pair<StorePath, ::Primitive>> values;);
    DefineAction(ToggleBool, NoMerge, "", StorePath path;);

    Json(Set, path, value);
    Json(SetMany, values);
    Json(ToggleBool, path);

    using Any = ActionVariant<Set, SetMany, ToggleBool>;
);
