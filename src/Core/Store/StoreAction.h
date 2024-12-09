#pragma once

#include "Core/Action/DefineAction.h"
#include "Patch/PatchJson.h"

DefineActionType(
    Store,
    DefineAction(ApplyPatch, Saved, CustomMerge, "", Patch patch;);
    Json(ApplyPatch, patch);

    using Any = ActionVariant<ApplyPatch>;
);
