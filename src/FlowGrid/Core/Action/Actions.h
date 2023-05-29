#pragma once

#include "App/AppAction.h"
#include "App/Audio/AudioAction.h"
#include "App/FileDialog/FileDialogAction.h"
#include "App/Style/StyleAction.h"
#include "Core/Store/StoreAction.h"

namespace Action {
// `Any` holds all action types.
using Any = Actionable::Combine<AppAction, AudioAction, FileDialogAction, StyleAction, StoreAction>::type;
using StatefulAction = Actionable::Filter<Actionable::IsSavable, Any>::type;
using NonStatefulAction = Actionable::Filter<Actionable::NotSavable, Any>::type;

// Composite action types.
using ActionMoment = std::pair<Any, TimePoint>;
using StatefulActionMoment = std::pair<StatefulAction, TimePoint>;
using Gesture = std::vector<StatefulActionMoment>;
using Gestures = std::vector<Gesture>;
} // namespace Action

namespace nlohmann {
DeclareJson(Action::StatefulAction);
} // namespace nlohmann
