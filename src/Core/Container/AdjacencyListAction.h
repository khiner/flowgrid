#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    AdjacencyList,
    DefineComponentAction(ToggleConnection, Saved, NoMerge, "", ID source; ID destination;);

    ComponentActionJson(ToggleConnection, source, destination);

    using Any = ActionVariant<ToggleConnection>;
);
