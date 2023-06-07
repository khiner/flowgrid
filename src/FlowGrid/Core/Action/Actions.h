#pragma once

#include "App/AppAction.h"
#include "App/Project/ProjectAction.h"

namespace Action {
// `Any` holds all action types.
using Any = Action::Combine<Project, App>::type;
using Stateful = Action::Filter<Action::IsSavable, Any>::type;
using NonStateful = Action::Filter<Action::IsNotSavable, Any>::type;

// Composite action types.
using ActionMoment = std::pair<Any, TimePoint>;
using StatefulActionMoment = std::pair<Stateful, TimePoint>;
using Gesture = std::vector<StatefulActionMoment>;
using Gestures = std::vector<Gesture>;
} // namespace Action

namespace nlohmann {
DeclareJson(Action::Stateful);
} // namespace nlohmann
