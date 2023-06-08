#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Style,

    DefineAction(SetImGuiColorPreset, Merge, "", int id;);
    DefineAction(SetImPlotColorPreset, Merge, "", int id;);
    DefineAction(SetFlowGridColorPreset, Merge, "", int id;);

    Json(SetImGuiColorPreset, id);
    Json(SetImPlotColorPreset, id);
    Json(SetFlowGridColorPreset, id);

    using Any = ActionVariant<SetImGuiColorPreset, SetImPlotColorPreset, SetFlowGridColorPreset>;
);
