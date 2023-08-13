#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Faust, DSP,
    DefineUnmergableAction(Create);
    DefineUnmergableAction(Delete, ID id;);

    Json(Create);
    Json(Delete, id);

    using Any = ActionVariant<Create, Delete>;
);
