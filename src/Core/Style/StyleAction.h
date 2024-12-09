#pragma once

#include "Core/Action/DefineAction.h"

DefineActionType(
    Style,

    DefineAction(SetImGuiColorPreset, Saved, Merge, "", int id;);
    DefineAction(SetImPlotColorPreset, Saved, Merge, "", int id;);
    DefineAction(SetProjectColorPreset, Saved, Merge, "", int id;);

    Json(SetImGuiColorPreset, id);
    Json(SetImPlotColorPreset, id);
    Json(SetProjectColorPreset, id);

    using Any = ActionVariant<SetImGuiColorPreset, SetImPlotColorPreset, SetProjectColorPreset>;
);
