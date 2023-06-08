#pragma once

#include "Core/Action/Action.h"
#include "Core/Json.h"

namespace Action {
Define(SetImGuiColorStyle, Merge, "", int id;);
Define(SetImPlotColorStyle, Merge, "", int id;);
Define(SetFlowGridColorStyle, Merge, "", int id;);

Json(SetImGuiColorStyle, id);
Json(SetImPlotColorStyle, id);
Json(SetFlowGridColorStyle, id);

using Style = ActionVariant<SetImGuiColorStyle, SetImPlotColorStyle, SetFlowGridColorStyle>;
} // namespace Action
