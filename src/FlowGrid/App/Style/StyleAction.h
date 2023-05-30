#pragma once

#include "Core/Action/Action.h"
#include "Core/Json.h"

namespace Action {
Define(SetImGuiColorStyle, 1, 0, Merge, "", int id;);
Define(SetImPlotColorStyle, 1, 0, Merge, "", int id;);
Define(SetFlowGridColorStyle, 1, 0, Merge, "", int id;);

Json(SetImGuiColorStyle, id);
Json(SetImPlotColorStyle, id);
Json(SetFlowGridColorStyle, id);

using StyleAction = ActionVariant<SetImGuiColorStyle, SetImPlotColorStyle, SetFlowGridColorStyle>;
} // namespace Action
