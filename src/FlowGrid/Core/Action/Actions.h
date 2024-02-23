#pragma once

#include "ActionMoment.h"

#include "Core/Container/ContainerAction.h"
#include "Core/Primitive/PrimitiveAction.h"
#include "Core/Store/StoreAction.h"
#include "Core/WindowsAction.h"
#include "Project/Audio/AudioAction.h"
#include "Project/FileDialog/FileDialogAction.h"
#include "Project/ProjectAction.h"
#include "Project/Style/StyleAction.h"
#include "Project/TextEditor/TextBufferAction.h"

namespace Action {
// `Any` holds all action types.
using Any = Combine<Primitive::Any, Container::Any, Project::Any, Store::Any, TextBuffer::Any, Audio::Any, FileDialog::Any, Windows::Any, Style::Any>;
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
inline static void to_json(json &j, const Action::Saved &action) {
    action.to_json(j);
}
inline static void from_json(const json &j, Action::Saved &action) {
    Action::Saved::from_json(j, action);
}

Json(SavedActionMoment, Action, QueueTime);
Json(Gesture, Actions, CommitTime);
} // namespace nlohmann
