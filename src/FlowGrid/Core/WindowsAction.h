#pragma once

#include "Action/DefineAction.h"

DefineActionType(
    Windows,
    DefineUnmergableAction(ToggleVisible, ID component_id;);

    Json(ToggleVisible, component_id);

    using Any = ActionVariant<ToggleVisible>;
);
