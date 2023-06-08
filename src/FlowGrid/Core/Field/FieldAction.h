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

DefineActionType(
    Vector,
    DefineAction(Set, CustomMerge, "", StorePath path; std::vector<::Primitive> value;);

    Json(Set, path, value);

    using Any = ActionVariant<Set>;
);

DefineActionType(
    Matrix,
    DefineAction(Set, CustomMerge, "", StorePath path; std::vector<::Primitive> data; Count row_count;);

    Json(Set, path, data, row_count);

    using Any = ActionVariant<Set>;
);
