#pragma once

#include "Action/DefineAction.h"

DefineActionType(
    Windows,
    DefineUnmergableAction(ToggleVisible, ID component_id;);
    DefineUnmergableAction(ToggleDebug, ID component_id;);

    Json(ToggleVisible, component_id);
    Json(ToggleDebug, component_id);

    using Any = ActionVariant<ToggleVisible, ToggleDebug>;
);
