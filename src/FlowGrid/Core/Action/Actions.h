#pragma once

#include "ActionMoment.h"

#include "Core/Container/ContainerAction.h"
#include "Core/Primitive/PrimitiveAction.h"
#include "Core/Primitive/TextBufferAction.h"
#include "Core/Store/StoreAction.h"
#include "Core/WindowsAction.h"
#include "Project/Audio/AudioAction.h"
#include "Project/FileDialog/FileDialogAction.h"
#include "Project/ProjectAction.h"
#include "Project/Style/StyleAction.h"

namespace Action {
// `Any` holds all action types.
using Any = Combine<Primitive::Any, Container::Any, Project::Any, Store::Any, TextBuffer::Any, Audio::Any, FileDialog::Any, Windows::Any, Style::Any>;
using Savable = Filter<Action::IsSavable, Any>;
using NonSavable = Filter<Action::IsNotSavable, Any>;
} // namespace Action

using SavableActionMoment = ActionMoment<Action::Savable>;
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
