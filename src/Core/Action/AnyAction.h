#pragma once

#include "Core/Style/StyleAction.h"
#include "Core/WindowsAction.h"
#include "CoreAction.h"
#include "FlowGridAction.h"
#include "Project/ProjectAction.h"

namespace Action {
// `Any` holds any action type.
//  - Metrics->Project->'Action variant size' shows the byte size of `Action::Any`.
using Any = Combine<Core::Any, Project::Any, Style::Any, Windows::Any, FlowGrid::Any>;
} // namespace Action
