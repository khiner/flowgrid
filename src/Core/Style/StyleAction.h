#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Style,

    DefineAction(SetImGuiColorPreset, Merge, "", int id;);
    DefineAction(SetImPlotColorPreset, Merge, "", int id;);
    DefineAction(SetProjectColorPreset, Merge, "", int id;);

    Json(SetImGuiColorPreset, id);
    Json(SetImPlotColorPreset, id);
    Json(SetProjectColorPreset, id);

    using Any = ActionVariant<SetImGuiColorPreset, SetImPlotColorPreset, SetProjectColorPreset>;
);
