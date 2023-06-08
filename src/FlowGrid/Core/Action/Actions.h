#pragma once

#include "App/AppAction.h"
#include "App/Project/ProjectAction.h"

namespace Action {
// `Any` holds all action types.
using Any = Combine<Project::Any, App::Any>::type;
using Stateful = Filter<Action::IsSavable, Any>::type;
using NonStateful = Filter<Action::IsNotSavable, Any>::type;

// Composite types.
using ActionMoment = std::pair<Any, TimePoint>;
using StatefulActionMoment = std::pair<Stateful, TimePoint>;
using Gesture = std::vector<StatefulActionMoment>;
using Gestures = std::vector<Gesture>;
} // namespace Action

namespace nlohmann {
DeclareJson(Action::Stateful);
} // namespace nlohmann
