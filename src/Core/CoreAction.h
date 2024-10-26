#pragma once

#include "Container/ContainerAction.h"
#include "FileDialog/FileDialogAction.h"
#include "Primitive/PrimitiveAction.h"
#include "Project/ProjectAction.h"
#include "Store/StoreAction.h"
#include "Style/StyleAction.h"
#include "TextEditor/TextBufferAction.h"
#include "WindowsAction.h"

namespace Action {
namespace Core {
using Any = Combine<Project::Any, FileDialog::Any, Primitive::Any, Container::Any, TextBuffer::Any, Store::Any, Windows::Any, Style::Any>;
} // namespace Core
} // namespace Action
