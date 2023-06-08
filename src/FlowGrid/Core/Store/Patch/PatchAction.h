#pragma once

#include "Core/Action/DefineAction.h"
#include "PatchJson.h"

DefineActionType(
    Patch,
    DefineAction(Apply, CustomMerge, "", ::Patch patch;);
    Json(Apply, patch);

    using Any = ActionVariant<Apply>;
);
