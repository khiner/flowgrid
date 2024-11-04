#pragma once

#include "Core/CoreAction.h"
#include "Core/FileDialog/FileDialogAction.h"
#include "Core/Store/StoreAction.h"
#include "Core/Style/StyleAction.h"
#include "Core/WindowsAction.h"
#include "FlowGridAction.h"
#include "Project/ProjectAction.h"

namespace Action {
// `Any` holds any action type.
//  - Metrics->Project->'Action variant size' shows the byte size of `Action::Any`.
using Any = Combine<Core::Any, Project::Any, FileDialog::Any, Style::Any, Windows::Any, Store::Any, FlowGrid::Any>;
} // namespace Action
