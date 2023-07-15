#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    FaustGraphStyle,
    DefineAction(ApplyColorPreset, Merge, "", int id;);
    DefineAction(ApplyLayoutPreset, Merge, "", int id;);

    Json(ApplyColorPreset, id);
    Json(ApplyLayoutPreset, id);

    using Any = ActionVariant<ApplyColorPreset, ApplyLayoutPreset>;
);
