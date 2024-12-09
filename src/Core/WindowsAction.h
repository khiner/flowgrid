#pragma once

#include "Action/DefineAction.h"

DefineActionType(
    Windows,
    DefineAction(ToggleVisible, Saved, NoMerge, "", ID component_id;);
    DefineAction(ToggleDebug, Saved, NoMerge, "", ID component_id;);

    Json(ToggleVisible, component_id);
    Json(ToggleDebug, component_id);

    using Any = ActionVariant<ToggleVisible, ToggleDebug>;
);
