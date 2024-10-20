#pragma once

#include "Core/TextEditor/TextBufferAction.h"
#include "Core/WindowsAction.h"
#include "FlowGrid/Audio/AudioAction.h"
#include "FlowGrid/Style/StyleAction.h"

namespace Action {
namespace State {
using Any = Combine<Windows::Any, Style::Any, TextBuffer::Any, Audio::Any>;
} // namespace State
} // namespace Action
