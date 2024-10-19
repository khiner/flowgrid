#pragma once

#include "Core/WindowsAction.h"
#include "FlowGrid/Audio/AudioAction.h"
#include "FlowGrid/Style/StyleAction.h"
#include "FlowGrid/TextEditor/TextBufferAction.h"

namespace Action {
namespace State {
using Any = Combine<Windows::Any, Style::Any, TextBuffer::Any, Audio::Any>;
} // namespace State
} // namespace Action
