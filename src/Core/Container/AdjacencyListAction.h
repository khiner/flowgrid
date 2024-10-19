#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    AdjacencyList,
    DefineUnmergableComponentAction(ToggleConnection, ID source; ID destination;);

    ComponentActionJson(ToggleConnection, source, destination);

    using Any = ActionVariant<ToggleConnection>;
);
