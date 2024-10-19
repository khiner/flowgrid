#pragma once

#include "Core/WindowsAction.h"
#include "Project/Audio/AudioAction.h"
#include "Project/Style/StyleAction.h"
#include "Project/TextEditor/TextBufferAction.h"

namespace Action {
namespace State {
using Any = Combine<Windows::Any, Style::Any, TextBuffer::Any, Audio::Any>;
} // namespace State
} // namespace Action
