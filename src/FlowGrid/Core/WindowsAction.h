#pragma once

#include "Action/DefineAction.h"

DefineActionType(
    Windows,
    DefineAction(ToggleVisible, NoMerge, "", ID component_id;);

    Json(ToggleVisible, component_id);

    using Any = ActionVariant<ToggleVisible>;
);
