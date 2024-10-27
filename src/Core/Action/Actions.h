#pragma once

#include "ActionMoment.h"
#include "Core/CoreAction.h"
#include "Core/FileDialog/FileDialogAction.h"

#include "Project/ProjectAction.h"
#include "StateAction.h"

namespace Action {
namespace State {
using Any = Combine<ProjectCore::Any, FlowGrid::Any>;
} // namespace State

// `Any` holds all action types.
using Any = Combine<Project::Any, FileDialog::Any, Core::Any, State::Any>;
using Saved = Filter<Action::IsSaved, Any>;
using NonSaved = Filter<Action::IsNotSaved, Any>;
} // namespace Action

using SavedActionMoment = ActionMoment<Action::Saved>;
using SavedActionMoments = std::vector<SavedActionMoment>;

struct Gesture {
    SavedActionMoments Actions;
    TimePoint CommitTime;
};

using Gestures = std::vector<Gesture>;

SavedActionMoments MergeActions(const SavedActionMoments &);

namespace nlohmann {
inline void to_json(json &j, const Action::Saved &action) {
    action.to_json(j);
}
inline void from_json(const json &j, Action::Saved &action) {
    Action::Saved::from_json(j, action);
}

Json(SavedActionMoment, Action, QueueTime);
Json(Gesture, Actions, CommitTime);
} // namespace nlohmann
