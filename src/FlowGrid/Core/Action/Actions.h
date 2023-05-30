#pragma once

#include "App/AppAction.h"
#include "App/Audio/AudioAction.h"
#include "App/FileDialog/FileDialogAction.h"
#include "App/Style/StyleAction.h"
#include "Core/Store/StoreAction.h"

namespace Action {
// `Any` holds all action types.
using Any = Action::Combine<AppAction, AudioAction, FileDialogAction, StyleAction, StoreAction>::type;
using StatefulAction = Action::Filter<Action::IsSavable, Any>::type;
using NonStatefulAction = Action::Filter<Action::NotSavable, Any>::type;

// Composite action types.
using ActionMoment = std::pair<Any, TimePoint>;
using StatefulActionMoment = std::pair<StatefulAction, TimePoint>;
using Gesture = std::vector<StatefulActionMoment>;
using Gestures = std::vector<Gesture>;
} // namespace Action

namespace nlohmann {
DeclareJson(Action::StatefulAction);
} // namespace nlohmann
