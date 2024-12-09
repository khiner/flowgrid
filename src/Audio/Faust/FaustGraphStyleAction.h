#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Faust, GraphStyle,
    DefineAction(ApplyColorPreset, Saved, Merge, "", int id;);
    DefineAction(ApplyLayoutPreset, Saved, Merge, "", int id;);

    Json(ApplyColorPreset, id);
    Json(ApplyLayoutPreset, id);

    using Any = ActionVariant<ApplyColorPreset, ApplyLayoutPreset>;
);
