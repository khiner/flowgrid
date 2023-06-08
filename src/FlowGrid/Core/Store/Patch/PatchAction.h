#pragma once

#include "Core/Action/Action.h"
#include "PatchJson.h"

namespace Action {
Define(ApplyPatch, CustomMerge, "", Patch patch;);

Json(ApplyPatch, patch);

using Patch = ActionVariant<ApplyPatch>;
} // namespace Action
