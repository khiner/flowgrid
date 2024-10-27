#pragma once

#include "FlowGridAction.h"
#include "ProjectCoreAction.h"

namespace Action {
namespace State {
using Any = Combine<ProjectCore::Any, FlowGrid::Any>;
} // namespace State
} // namespace Action
