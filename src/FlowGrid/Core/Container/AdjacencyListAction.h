#pragma once

#include "Core/Action/DefineAction.h"

#include "Core/Primitive/Scalar.h"

DefineActionType(
    AdjacencyList,
    // todo NoMerge 
    DefineFieldAction(ToggleConnection, "", ID source; ID destination;);

    Json(ToggleConnection, path, source, destination);

    using Any = ActionVariant<ToggleConnection>;
);
