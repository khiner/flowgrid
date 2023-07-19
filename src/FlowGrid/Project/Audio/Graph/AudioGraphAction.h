#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    AudioGraph,
    DefineAction(DeleteNode, NoMerge, "", u32 id;);

    Json(DeleteNode, id);

    using Any = ActionVariant<DeleteNode>;
);
