#pragma once

#include "App/AppAction.h"
#include "App/ProjectAction.h"

namespace Action {
// `Any` holds all action types.
using Any = Action::Combine<ProjectAction, AppAction>::type;
using StatefulAction = Action::Filter<Action::IsSavable, Any>::type;
using NonStatefulAction = Action::Filter<Action::IsNotSavable, Any>::type;

// Composite action types.
using ActionMoment = std::pair<Any, TimePoint>;
using StatefulActionMoment = std::pair<StatefulAction, TimePoint>;
using Gesture = std::vector<StatefulActionMoment>;
using Gestures = std::vector<Gesture>;
} // namespace Action

namespace nlohmann {
DeclareJson(Action::StatefulAction);
} // namespace nlohmann
