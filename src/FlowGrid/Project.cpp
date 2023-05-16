#include "Project.h"

#include "immer/map.hpp"
#include "immer/map_transient.hpp"
#include <range/v3/core.hpp>
#include <range/v3/view/join.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/transform.hpp>

#include "App.h"
#include "FileDialog/FileDialogDataJson.h"
#include "Helper/File.h"
#include "Store/StoreHistory.h"

static std::optional<fs::path> CurrentProjectPath;
static bool ProjectHasChanges{false};

// Main setter to modify the canonical application state store.
// _All_ store assignments happen via this method.
Patch SetStore(const Store &store) {
    const auto &patch = store::CreatePatch(AppStore, store);
    if (patch.Empty()) return {};

    store::Set(store);
    History.LatestUpdatedPaths = patch.Ops | views::transform([&patch](const auto &entry) { return patch.BasePath / entry.first; }) | to<vector>;
    ProjectHasChanges = true;

    static std::set<Base *> modified_fields;
    modified_fields.clear();
    for (const auto &path : History.LatestUpdatedPaths) {
        // Find all updated fields, including container fields.
        auto modified_field = Base::WithPath.find(path);
        if (modified_field == Base::WithPath.end()) modified_field = Base::WithPath.find(path.parent_path());
        if (modified_field == Base::WithPath.end()) modified_field = Base::WithPath.find(path.parent_path().parent_path());
        if (modified_field == Base::WithPath.end()) throw std::runtime_error(std::format("`SetStore` resulted in a patch affecting a path belonging to an unknown field: {}", path.string()));

        modified_fields.emplace(modified_field->second);

        // Setting `ImGuiSettings` does not require a `s.Apply` on the action, since the action will be initiated by ImGui itself,
        // whereas the style editors don't update the ImGui/ImPlot contexts themselves.
        if (path.string().rfind(imgui_settings.Path.string(), 0) == 0) UiContext.ApplyFlags |= UIContext::Flags_ImGuiSettings; // TODO only when not ui-initiated
        else if (path.string().rfind(fg::style.ImGui.Path.string(), 0) == 0) UiContext.ApplyFlags |= UIContext::Flags_ImGuiStyle;
        else if (path.string().rfind(fg::style.ImPlot.Path.string(), 0) == 0) UiContext.ApplyFlags |= UIContext::Flags_ImPlotStyle;
    }
    for (auto *modified_field : modified_fields) modified_field->Update();

    return patch;
}

void SetHistoryIndex(Count index) {
    History.SetIndex(index);
    SetStore(History.CurrentStore());
}

bool Project::IsUserProjectPath(const fs::path &path) {
    return fs::relative(path).string() != fs::relative(EmptyProjectPath).string() &&
        fs::relative(path).string() != fs::relative(DefaultProjectPath).string();
}

void Project::SaveEmptyProject() { SaveProject(EmptyProjectPath); }
void Project::SaveCurrentProject() {
    if (CurrentProjectPath) SaveProject(*CurrentProjectPath);
}

void Project::Init() {
    CurrentProjectPath = {};
    ProjectHasChanges = false;
    History = {AppStore};
    UiContext.IsWidgetGesturing = false;
}

std::optional<StoreJsonFormat> GetStoreJsonFormat(const fs::path &path) {
    const string &ext = path.extension();
    if (StoreJsonFormatForExtension.contains(ext)) return StoreJsonFormatForExtension.at(ext);
    return {};
}

#include "AppPreferences.h"

void SetCurrentProjectPath(const fs::path &path) {
    ProjectHasChanges = false;
    CurrentProjectPath = path;
    Preferences.OnProjectOpened(path);
}

#include "Store/StoreJson.h"

void Project::OpenProject(const fs::path &path) {
    const auto format = GetStoreJsonFormat(path);
    if (!format) return; // TODO log

    Init();

    const nlohmann::json project = nlohmann::json::parse(FileIO::read(path));
    if (format == StateFormat) {
        SetStore(JsonToStore(project));
    } else if (format == ActionFormat) {
        OpenProject(EmptyProjectPath);

        const auto &[gestures, index] = JsonToGestures(project);
        auto transient = AppStore.transient();
        for (const auto &gesture : gestures) {
            const auto before_store = transient.persistent();
            for (const auto &action_moment : gesture) {
                s.Update(action_moment.first, transient);
            }
            const auto after_store = transient.persistent();
            const auto &patch = store::CreatePatch(before_store, after_store);
            const auto &gesture_time = gesture.back().second;
            History.Add(gesture_time, after_store, gesture); // todo save/load gesture commit times
            for (const auto &[partial_path, op] : patch.Ops) {
                History.CommittedUpdateTimesForPath[patch.BasePath / partial_path].emplace_back(gesture_time);
            }
        }
        SetStore(transient.persistent());
        ::SetHistoryIndex(index);
    }

    if (IsUserProjectPath(path)) SetCurrentProjectPath(path);
}

bool Project::SaveProject(const fs::path &path) {
    const bool is_current_project = CurrentProjectPath && fs::equivalent(path, *CurrentProjectPath);
    if (is_current_project && !ActionAllowed(action::id<Actions::SaveCurrentProject>)) return false;

    const auto format = GetStoreJsonFormat(path);
    if (!format) return false; // TODO log

    History.FinalizeGesture(); // Make sure any pending actions/diffs are committed.
    if (!FileIO::write(path, GetStoreJson(*format).dump())) return false; // TODO log

    if (IsUserProjectPath(path)) SetCurrentProjectPath(path);
    return true;
}

#include "Audio/Faust/FaustGraph.h"

// todo there's some weird circular dependency type thing going on here.
//   I should be able to define this inside `Project.h` and not include `Action.h` here,
//   but when I do, it compiles but with invisible issues around `Match` not working with `ProjectAction`.
// static void ApplyAction(const ProjectAction &);
static void ApplyAction(const ProjectAction &action) {
    Match(
        action,
        // Handle actions that don't directly update state.
        // These options don't get added to the action/gesture history, since they only have non-application side effects,
        // and we don't want them replayed when loading a saved `.fga` project.
        [&](const Actions::OpenEmptyProject &) { Project::OpenProject(EmptyProjectPath); },
        [&](const Actions::OpenProject &a) { Project::OpenProject(a.path); },
        [&](const Actions::OpenDefaultProject &) { Project::OpenProject(DefaultProjectPath); },

        [&](const Actions::SaveProject &a) { Project::SaveProject(a.path); },
        [&](const Actions::SaveDefaultProject &) { Project::SaveProject(DefaultProjectPath); },
        [&](const Actions::SaveCurrentProject &) { Project::SaveCurrentProject(); },
        [&](const Actions::SaveFaustFile &a) { FileIO::write(a.path, s.Audio.Faust.Code); },
        [](const Actions::SaveFaustSvgFile &a) { SaveBoxSvg(a.path); },

        // `History.Index`-changing actions:
        [&](const Actions::Undo &) {
            if (History.Empty()) return;

            // `StoreHistory::SetIndex` reverts the current gesture before applying the new history index.
            // If we're at the end of the stack, we want to finalize the active gesture and add it to the stack.
            // Otherwise, if we're already in the middle of the stack somewhere, we don't want an active gesture
            // to finalize and cut off everything after the current history index, so an undo just ditches the active changes.
            // (This allows consistent behavior when e.g. being in the middle of a change and selecting a point in the undo history.)
            if (History.Index == History.Size() - 1) {
                if (!History.ActiveGesture.empty()) History.FinalizeGesture();
                ::SetHistoryIndex(History.Index - 1);
            } else {
                ::SetHistoryIndex(History.Index - (History.ActiveGesture.empty() ? 1 : 0));
            }
        },
        [&](const Actions::Redo &) { ::SetHistoryIndex(History.Index + 1); },
        [&](const Actions::SetHistoryIndex &a) { ::SetHistoryIndex(a.index); },
    );
}

//-----------------------------------------------------------------------------
// [SECTION] Action queueing
//-----------------------------------------------------------------------------

bool ActionAllowed(const ActionID id) {
    switch (id) {
        case action::id<Actions::Undo>: return History.CanUndo();
        case action::id<Actions::Redo>: return History.CanRedo();
        case action::id<Actions::OpenDefaultProject>: return fs::exists(DefaultProjectPath);
        case action::id<Actions::SaveProject>:
        case action::id<Actions::SaveDefaultProject>: return !History.Empty();
        case action::id<Actions::ShowSaveProjectDialog>:
            // If there is no current project, `SaveCurrentProject` will be transformed into a `ShowSaveProjectDialog`.
        case action::id<Actions::SaveCurrentProject>: return ProjectHasChanges;
        case action::id<Actions::OpenFileDialog>: return !s.FileDialog.Visible;
        case action::id<Actions::CloseFileDialog>: return s.FileDialog.Visible;
        default: return true;
    }
}
bool ActionAllowed(const Action &action) { return ActionAllowed(action::GetId(action)); }
bool ActionAllowed(const EmptyAction &action) {
    return std::visit([&](Action &&a) { return ActionAllowed(a); }, action);
}

#include "blockingconcurrentqueue.h"

using action::ActionMoment, action::StateActionMoment;
inline static moodycamel::BlockingConcurrentQueue<ActionMoment> ActionQueue;

void Project::RunQueuedActions(bool force_finalize_gesture) {
    static ActionMoment action_moment;
    static vector<StateActionMoment> state_actions; // Same type as `Gesture`, but doesn't represent a full semantic "gesture".
    state_actions.clear();

    auto transient = AppStore.transient();
    while (ActionQueue.try_dequeue(action_moment)) {
        // Note that multiple actions enqueued during the same frame (in the same queue batch) are all evaluated independently to see if they're allowed.
        // This means that if one action would change the state such that a later action in the same batch _would be allowed_,
        // the current approach would incorrectly throw this later action away.
        auto &[action, _] = action_moment;
        if (!ActionAllowed(action)) continue;

        // Special cases:
        // * If saving the current project where there is none, open the save project dialog so the user can tell us where to save it:
        if (std::holds_alternative<Actions::SaveCurrentProject>(action) && !CurrentProjectPath) action = Actions::ShowSaveProjectDialog{};
        // * Treat all toggles as immediate actions. Otherwise, performing two toggles in a row compresses into nothing:
        force_finalize_gesture |= std::holds_alternative<ToggleValue>(action);

        Match(
            action,
            [&](const ProjectAction &a) {
                ApplyAction(a);
            },
            [&](const StateAction &a) {
                s.Update(a, transient);
                state_actions.emplace_back(a, action_moment.second);
            },
        );
    }

    const bool finalize = force_finalize_gesture || (!UiContext.IsWidgetGesturing && !History.ActiveGesture.empty() && History.GestureTimeRemainingSec(s.ApplicationSettings.GestureDurationSec) <= 0);
    if (!state_actions.empty()) {
        const auto &patch = SetStore(transient.persistent());
        History.ActiveGesture.insert(History.ActiveGesture.end(), state_actions.begin(), state_actions.end());
        History.UpdateGesturePaths(state_actions, patch);
    }
    if (finalize) History.FinalizeGesture();
}

bool q(Action &&action, bool flush) {
    ActionQueue.enqueue({action, Clock::now()});
    if (flush) Project::RunQueuedActions(true); // If the `flush` flag is set, we finalize the gesture now.
    return true;
}