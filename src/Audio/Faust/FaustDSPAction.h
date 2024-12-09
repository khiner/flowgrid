#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Faust, DSP,
    DefineAction(Create, Saved, NoMerge, "");
    DefineAction(Delete, Saved, NoMerge, "", ID id;);

    Json(Create);
    Json(Delete, id);

    using Any = ActionVariant<Create, Delete>;
);
