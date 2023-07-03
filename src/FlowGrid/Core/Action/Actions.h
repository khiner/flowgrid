#pragma once

#include "App/Audio/AudioAction.h"
#include "App/FileDialog/FileDialogAction.h"
#include "App/Project/ProjectAction.h"
#include "App/Style/StyleAction.h"
#include "Core/Field/FieldAction.h"
#include "Core/Store/StoreAction.h"
#include "Core/WindowsAction.h"

#include "Helper/Time.h"

namespace Action {
// `Any` holds all action types.
using Any = Combine<Project::Any, Field::Any, Store::Any, Audio::Any, FileDialog::Any, Windows::Any, Style::Any>;
using Savable = Filter<Action::IsSavable, Any>;
using NonSavable = Filter<Action::IsNotSavable, Any>;
} // namespace Action

// Action moments are actions paired with the time they were queued.
struct ActionMoment {
    Action::Any Action;
    TimePoint QueueTime;
};

struct SavableActionMoment {
    Action::Savable Action;
    TimePoint QueueTime;
};

using SavableActionMoments = std::vector<SavableActionMoment>;

struct Gesture {
    SavableActionMoments Actions;
    TimePoint CommitTime;
};

using Gestures = std::vector<Gesture>;

SavableActionMoments MergeActions(const SavableActionMoments &);

namespace nlohmann {
inline static void to_json(json &j, const Action::Savable &action) {
    action.to_json(j);
}
inline static void from_json(const json &j, Action::Savable &action) {
    Action::Savable::from_json(j, action);
}

Json(SavableActionMoment, Action, QueueTime);
Json(Gesture, Actions, CommitTime);
} // namespace nlohmann
