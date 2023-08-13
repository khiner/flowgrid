#pragma once

#include "Core/Action/DefineAction.h"

DefineNestedActionType(
    Faust, GraphStyle,
    DefineAction(ApplyColorPreset, Merge, "", int id;);
    DefineAction(ApplyLayoutPreset, Merge, "", int id;);

    Json(ApplyColorPreset, id);
    Json(ApplyLayoutPreset, id);

    using Any = ActionVariant<ApplyColorPreset, ApplyLayoutPreset>;
);
