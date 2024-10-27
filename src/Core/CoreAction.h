#pragma once

#include "Container/ContainerAction.h"
#include "Primitive/PrimitiveAction.h"
#include "Store/StoreAction.h"
#include "TextEditor/TextBufferAction.h"

namespace Action {
namespace Core {
using Any = Combine<Primitive::Any, Container::Any, TextBuffer::Any, Store::Any>;
} // namespace Core
} // namespace Action
