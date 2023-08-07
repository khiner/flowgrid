#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    AdjacencyList,
    DefineUnmergableFieldAction(ToggleConnection, ID source; ID destination;);

    Json(ToggleConnection, path, source, destination);

    using Any = ActionVariant<ToggleConnection>;
);
