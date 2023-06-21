#pragma once

#include "App/AppAction.h"
#include "App/Project/ProjectAction.h"

namespace Action {
// `Any` holds all action types.
using Any = Combine<Project::Any, App::Any>::type;
using Savable = Filter<Action::IsSavable, Any>::type;
using NonSavable = Filter<Action::IsNotSavable, Any>::type;

// Action moments are actions paired with the time they were queued.
using ActionMoment = std::pair<Any, TimePoint>;
using SavableActionMoment = std::pair<Savable, TimePoint>;
using Gesture = std::vector<SavableActionMoment>;
using Gestures = std::vector<Gesture>;
} // namespace Action

namespace nlohmann {
DeclareJson(Action::Savable);
} // namespace nlohmann
