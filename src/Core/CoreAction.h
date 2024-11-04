#pragma once

#include "Container/ContainerAction.h"
#include "Primitive/PrimitiveAction.h"
#include "TextEditor/TextBufferAction.h"

namespace Action {
namespace Core {
using Any = Combine<Primitive::Any, Container::Any, TextBuffer::Any>;
} // namespace Core
} // namespace Action
