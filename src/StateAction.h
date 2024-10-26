#pragma once

#include "Audio/AudioAction.h"
#include "Core/WindowsAction.h"
#include "Style/StyleAction.h"

namespace Action {
namespace State {
using Any = Combine<Windows::Any, Style::Any, Audio::Any>;
} // namespace State
} // namespace Action
