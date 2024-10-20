#pragma once

#include "Container/ContainerAction.h"
#include "FileDialog/FileDialogAction.h"
#include "Primitive/PrimitiveAction.h"
#include "Project/ProjectAction.h"
#include "Store/StoreAction.h"

namespace Action {
namespace Core {
using Any = Combine<Project::Any, Primitive::Any, Container::Any, Store::Any, FileDialog::Any>;
} // namespace Core
} // namespace Action
