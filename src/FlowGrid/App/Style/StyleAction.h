#pragma once

#include "Core/Action/Actionable.h"
#include "Core/Json.h"

namespace Action {
using namespace Actionable;

Define(SetImGuiColorStyle, 1, 0, Merge, "", int id;);
Define(SetImPlotColorStyle, 1, 0, Merge, "", int id;);
Define(SetFlowGridColorStyle, 1, 0, Merge, "", int id;);

Json(SetImGuiColorStyle, id);
Json(SetImPlotColorStyle, id);
Json(SetFlowGridColorStyle, id);

using StyleAction = ActionVariant<SetImGuiColorStyle, SetImPlotColorStyle, SetFlowGridColorStyle>;
} // namespace Action
