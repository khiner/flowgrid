#pragma once

#include "Core/Container/ContainerAction.h"
#include "Core/FileDialog/FileDialogAction.h"
#include "Core/Primitive/PrimitiveAction.h"
#include "Core/Store/StoreAction.h"
#include "Core/Style/StyleAction.h"
#include "Core/TextEditor/TextBufferAction.h"
#include "Core/WindowsAction.h"
#include "FlowGridAction.h"
#include "Project/ProjectAction.h"

namespace Action {
// `Any` holds any action type.
//  - Metrics->Project->'Action variant size' shows the byte size of `Action::Any`.
using Any = Combine<Primitive::Any, Container::Any, TextBuffer::Any, Project::Any, FileDialog::Any, Style::Any, Windows::Any, Store::Any, FlowGrid::Any>;
} // namespace Action
