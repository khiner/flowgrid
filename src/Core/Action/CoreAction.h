#pragma once

#include "Core/Container/ContainerAction.h"
#include "Core/FileDialog/FileDialogAction.h"
#include "Core/Primitive/PrimitiveAction.h"
#include "Core/Store/StoreAction.h"
#include "Core/TextEditor/TextBufferAction.h"

namespace Action {
namespace Core {
using Any = Combine<FileDialog::Any, Primitive::Any, Container::Any, TextBuffer::Any, Store::Any>;
} // namespace Core
} // namespace Action
