#include "AppContext.h"

//-----------------------------------------------------------------------------
// [SECTION] State JSON
//-----------------------------------------------------------------------------

#include "blockingconcurrentqueue.h"
#include <range/v3/view/concat.hpp>

#include "AppPreferences.h"
#include "Helper/File.h"
#include "StateJson.h"
#include "UI/Faust/FaustGraph.h"

namespace nlohmann {
inline void to_json(json &j, const Store &v) {
    for (const auto &[key, value] : v) j[json::json_pointer(key.string())] = value;
}
} // namespace nlohmann

// `from_json` defined out of `nlohmann`, to be called manually.
// This avoids getting a reference arg to a default-constructed, non-transient `Store` instance.
Store store_from_json(const json &j) {
    const auto &flattened = j.flatten();
    StoreEntries entries(flattened.size());
    int item_index = 0;
    for (const auto &[key, value] : flattened.items()) entries[item_index++] = {StatePath(key), Primitive(value)};

    TransientStore store;
    for (const auto &[path, value] : entries) store.set(path, value);
    return store.persistent();
}

//-----------------------------------------------------------------------------
// [SECTION] Main state update method
//-----------------------------------------------------------------------------

void State::Update(const StateAction &action, TransientStore &store) const {
    Match(
        action,
        [&store](const SetValue &a) { store.set(a.path, a.value); },
        [&store](const SetValues &a) { ::Set(a.values, store); },
        [&store](const SetVector &a) { ::Set(a.path, a.value, store); },
        [&store](const SetMatrix &a) { ::Set(a.path, a.data, a.row_count, store); },
        [&store](const ToggleValue &a) { store.set(a.path, !std::get<bool>(AppStore.at(a.path))); },
        [&store](const ApplyPatch &a) {
            const auto &patch = a.patch;
            for (const auto &[partial_path, op] : patch.Ops) {
                const auto &path = patch.BasePath / partial_path;
                if (op.Op == AddOp || op.Op == ReplaceOp) store.set(path, *op.Value);
                else if (op.Op == RemoveOp) store.erase(path);
            }
        },
        [&](const OpenFileDialog &a) { FileDialog.Set(json::parse(a.dialog_json), store); },
        [&](const CloseFileDialog &) { Set(FileDialog.Visible, false, store); },
        [&](const ShowOpenProjectDialog &) { FileDialog.Set({"Choose file", AllProjectExtensionsDelimited, ".", ""}, store); },
        [&](const ShowSaveProjectDialog &) { FileDialog.Set({"Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1}, store); },
        [&](const ShowOpenFaustFileDialog &) { FileDialog.Set({"Choose file", FaustDspFileExtension, ".", ""}, store); },
        [&](const ShowSaveFaustFileDialog &) { FileDialog.Set({"Choose file", FaustDspFileExtension, ".", "my_dsp", true, 1}, store); },
        [&](const ShowSaveFaustSvgFileDialog &) { FileDialog.Set({"Choose directory", ".*", ".", "faust_graph", true, 1}, store); },

        // todo enum types instead of raw integers
        [&](const SetImGuiColorStyle &a) {
            switch (a.id) {
                case 0: return Style.ImGui.ColorsDark(store);
                case 1: return Style.ImGui.ColorsLight(store);
                case 2: return Style.ImGui.ColorsClassic(store);
            }
        },
        [&](const SetImPlotColorStyle &a) {
            switch (a.id) {
                case 0: return Style.ImPlot.ColorsAuto(store);
                case 1: return Style.ImPlot.ColorsDark(store);
                case 2: return Style.ImPlot.ColorsLight(store);
                case 3: return Style.ImPlot.ColorsClassic(store);
            }
        },
        [&](const SetFlowGridColorStyle &a) {
            switch (a.id) {
                case 0: return Style.FlowGrid.ColorsDark(store);
                case 1: return Style.FlowGrid.ColorsLight(store);
                case 2: return Style.FlowGrid.ColorsClassic(store);
            }
        },
        [&](const SetGraphColorStyle &a) {
            switch (a.id) {
                case 0: return Style.FlowGrid.Graph.ColorsDark(store);
                case 1: return Style.FlowGrid.Graph.ColorsLight(store);
                case 2: return Style.FlowGrid.Graph.ColorsClassic(store);
                case 3: return Style.FlowGrid.Graph.ColorsFaust(store);
            }
        },
        [&](const SetGraphLayoutStyle &a) {
            switch (a.id) {
                case 0: return Style.FlowGrid.Graph.LayoutFlowGrid(store);
                case 1: return Style.FlowGrid.Graph.LayoutFaust(store);
            }
        },
        [&](const OpenFaustFile &a) { Set(Faust.Code, FileIO::read(a.path), store); },
        [&](const CloseApplication &) { Set({{UiProcess.Running, false}, {Audio.Device.On, false}}, store); },
    );
}

//-----------------------------------------------------------------------------
// [SECTION] Context
//-----------------------------------------------------------------------------

Context::Context() {
    InitStore = {}; // Transient store only used for `State` construction, so we can clear it to save memory.
}

Context::~Context() = default;

bool Context::IsUserProjectPath(const fs::path &path) {
    return fs::relative(path).string() != fs::relative(EmptyProjectPath).string() &&
        fs::relative(path).string() != fs::relative(DefaultProjectPath).string();
}

void Context::SaveEmptyProject() { SaveProject(EmptyProjectPath); }
void Context::SaveCurrentProject() {
    if (CurrentProjectPath) SaveProject(*CurrentProjectPath);
}

json Context::GetProjectJson(const ProjectFormat format) {
    switch (format) {
        case StateFormat: return AppStore;
        case ActionFormat: return {{"gestures", History.Gestures()}, {"index", History.Index}};
    }
}

void Context::Clear() {
    CurrentProjectPath = {};
    ProjectHasChanges = false;
    History = {ApplicationStore};
    UiContext.IsWidgetGesturing = false;
}

std::optional<ProjectFormat> GetProjectFormat(const fs::path &path) {
    const string &ext = path.extension();
    if (ProjectFormatForExtension.contains(ext)) return ProjectFormatForExtension.at(ext);
    return {};
}

Patch Context::SetStore(const Store &store) {
    const auto &patch = CreatePatch(AppStore, store);
    if (patch.Empty()) return {};

    ApplicationStore = store; // This is the only place `ApplicationStore` is modified.
    History.LatestUpdatedPaths = patch.Ops | transform([&patch](const auto &entry) { return patch.BasePath / entry.first; }) | to<vector>;
    ProjectHasChanges = true;

    static std::set<Base *> modified_fields;
    modified_fields.clear();
    for (const auto &path : History.LatestUpdatedPaths) {
        // Find all updated fields, including container fields.
        auto modified_field = Base::WithPath.find(path);
        if (modified_field == Base::WithPath.end()) modified_field = Base::WithPath.find(path.parent_path());
        if (modified_field == Base::WithPath.end()) modified_field = Base::WithPath.find(path.parent_path().parent_path());
        if (modified_field == Base::WithPath.end()) throw std::runtime_error(fmt::format("`SetStore` resulted in a patch affecting a path belonging to an unknown field: {}", path.string()));

        modified_fields.emplace(modified_field->second);

        // Setting `ImGuiSettings` does not require a `s.Apply` on the action, since the action will be initiated by ImGui itself,
        // whereas the style editors don't update the ImGui/ImPlot contexts themselves.
        if (path.string().rfind(s.ImGuiSettings.Path.string(), 0) == 0) UiContext.ApplyFlags |= UIContext::Flags_ImGuiSettings; // TODO only when not ui-initiated
        else if (path.string().rfind(s.Style.ImGui.Path.string(), 0) == 0) UiContext.ApplyFlags |= UIContext::Flags_ImGuiStyle;
        else if (path.string().rfind(s.Style.ImPlot.Path.string(), 0) == 0) UiContext.ApplyFlags |= UIContext::Flags_ImPlotStyle;
    }
    for (auto *modified_field : modified_fields) modified_field->Update();

    return patch;
}

void Context::OpenProject(const fs::path &path) {
    const auto format = GetProjectFormat(path);
    if (!format) return; // TODO log

    Clear();

    const json project = json::parse(FileIO::read(path));
    if (format == StateFormat) {
        SetStore(store_from_json(project));
    } else if (format == ActionFormat) {
        OpenProject(EmptyProjectPath);

        const Gestures gestures = project["gestures"];
        auto transient = AppStore.transient();
        for (const auto &gesture : gestures) {
            const auto before_store = transient.persistent();
            for (const auto &action_moment : gesture) {
                s.Update(action_moment.first, transient);
            }
            const auto after_store = transient.persistent();
            const auto &patch = CreatePatch(before_store, after_store);
            const auto &gesture_time = gesture.back().second;
            History.Records.push_back({gesture_time, after_store, gesture}); // todo save/load gesture commit times
            History.Index = History.Size() - 1;
            for (const auto &[partial_path, op] : patch.Ops) History.CommittedUpdateTimesForPath[patch.BasePath / partial_path].emplace_back(gesture_time);
        }
        SetStore(transient.persistent());
        History.SetIndex(project["index"]);
    }

    if (IsUserProjectPath(path)) SetCurrentProjectPath(path);
}

bool Context::SaveProject(const fs::path &path) {
    const bool is_current_project = CurrentProjectPath && fs::equivalent(path, *CurrentProjectPath);
    if (is_current_project && !ActionAllowed(action::id<Actions::SaveCurrentProject>)) return false;

    const auto format = GetProjectFormat(path);
    if (!format) return false; // TODO log

    History.FinalizeGesture(); // Make sure any pending actions/diffs are committed.
    if (!FileIO::write(path, GetProjectJson(*format).dump())) return false; // TODO log

    if (IsUserProjectPath(path)) SetCurrentProjectPath(path);
    return true;
}

void Context::SetCurrentProjectPath(const fs::path &path) {
    ProjectHasChanges = false;
    CurrentProjectPath = path;
    Preferences.SetCurrentProjectPath(path);
}

bool Context::ActionAllowed(const ActionID id) const {
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
bool Context::ActionAllowed(const Action &action) const { return ActionAllowed(action::GetId(action)); }
bool Context::ActionAllowed(const EmptyAction &action) const {
    return std::visit([&](Action &&a) { return ActionAllowed(a); }, action);
}

void Context::ApplyAction(const ProjectAction &action) {
    Match(
        action,
        // Handle actions that don't directly update state.
        // These options don't get added to the action/gesture history, since they only have non-application side effects,
        // and we don't want them replayed when loading a saved `.fga` project.
        [&](const Actions::OpenEmptyProject &) { OpenProject(EmptyProjectPath); },
        [&](const Actions::OpenProject &a) { OpenProject(a.path); },
        [&](const Actions::OpenDefaultProject &) { OpenProject(DefaultProjectPath); },

        [&](const Actions::SaveProject &a) { SaveProject(a.path); },
        [&](const Actions::SaveDefaultProject &) { SaveProject(DefaultProjectPath); },
        [&](const Actions::SaveCurrentProject &) { SaveCurrentProject(); },
        [&](const Actions::SaveFaustFile &a) { FileIO::write(a.path, s.Faust.Code); },
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
                History.SetIndex(History.Index - 1);
            } else {
                History.SetIndex(History.Index - (History.ActiveGesture.empty() ? 1 : 0));
            }
        },
        [&](const Actions::Redo &) { History.SetIndex(History.Index + 1); },
        [&](const Actions::SetHistoryIndex &a) { History.SetIndex(a.index); },
    );
}

//-----------------------------------------------------------------------------
// [SECTION] History
//-----------------------------------------------------------------------------

Count StoreHistory::Size() const { return Records.size(); }
bool StoreHistory::Empty() const { return Size() <= 1; } // There is always an initial store in the history records.
bool StoreHistory::CanUndo() const { return !ActiveGesture.empty() || Index > 0; }
bool StoreHistory::CanRedo() const { return Index < Size() - 1; }

Gestures StoreHistory::Gestures() const {
    return Records | transform([](const auto &record) { return record.Gesture; }) |
        views::filter([](const auto &gesture) { return !gesture.empty(); }) | to<vector>; // First gesture is expected to be empty.
}
TimePoint StoreHistory::GestureStartTime() const {
    if (ActiveGesture.empty()) return {};
    return ActiveGesture.back().second;
}

float StoreHistory::GestureTimeRemainingSec() const {
    if (ActiveGesture.empty()) return 0;
    return std::max(0.f, s.ApplicationSettings.GestureDurationSec - fsec(Clock::now() - GestureStartTime()).count());
}

void StoreHistory::FinalizeGesture() {
    if (ActiveGesture.empty()) return;

    const auto merged_gesture = action::MergeGesture(ActiveGesture);
    ActiveGesture.clear();
    GestureUpdateTimesForPath.clear();
    if (merged_gesture.empty()) return;

    const auto &patch = CreatePatch(AppStore, Records[Index].Store);
    if (patch.Empty()) return;

    while (Size() > Index + 1) Records.pop_back(); // TODO use an undo _tree_ and keep this history
    Records.push_back({Clock::now(), AppStore, merged_gesture});
    Index = Size() - 1;
    const auto &gesture_time = merged_gesture.back().second;
    for (const auto &[partial_path, op] : patch.Ops) CommittedUpdateTimesForPath[patch.BasePath / partial_path].emplace_back(gesture_time);
}

void StoreHistory::UpdateGesturePaths(const Gesture &gesture, const Patch &patch) {
    const auto &gesture_time = gesture.back().second;
    for (const auto &[partial_path, op] : patch.Ops) GestureUpdateTimesForPath[patch.BasePath / partial_path].emplace_back(gesture_time);
}

std::optional<TimePoint> StoreHistory::LatestUpdateTime(const StatePath &path) const {
    if (GestureUpdateTimesForPath.contains(path)) return GestureUpdateTimesForPath.at(path).back();
    if (CommittedUpdateTimesForPath.contains(path)) return CommittedUpdateTimesForPath.at(path).back();
    return {};
}

StoreHistory::Plottable StoreHistory::StatePathUpdateFrequencyPlottable() const {
    const std::set<StatePath> paths = views::concat(views::keys(CommittedUpdateTimesForPath), views::keys(GestureUpdateTimesForPath)) | to<std::set>;
    if (paths.empty()) return {};

    const bool has_gesture = !GestureUpdateTimesForPath.empty();
    vector<ImU64> values(has_gesture ? paths.size() * 2 : paths.size());
    Count i = 0;
    for (const auto &path : paths) values[i++] = CommittedUpdateTimesForPath.contains(path) ? CommittedUpdateTimesForPath.at(path).size() : 0;
    // Optionally add a second plot item for gesturing update times. See `ImPlot::PlotBarGroups` for value ordering explanation.
    if (has_gesture)
        for (const auto &path : paths) values[i++] = GestureUpdateTimesForPath.contains(path) ? GestureUpdateTimesForPath.at(path).size() : 0;

    const auto labels = paths | transform([](const string &path) {
                            // Convert `string` to char array, removing first character of the path, which is a '/'.
                            char *label = new char[path.size()];
                            std::strcpy(label, string{path.begin() + 1, path.end()}.c_str());
                            return label;
                        }) |
        to<vector<const char *>>;

    return {labels, values};
}

void StoreHistory::SetIndex(Count new_index) {
    // If we're mid-gesture, revert the current gesture before navigating to the requested history index.
    if (!ActiveGesture.empty()) {
        ActiveGesture.clear();
        GestureUpdateTimesForPath.clear();
        c.SetStore(Records[Index].Store);
    }
    if (new_index == Index || new_index < 0 || new_index >= Size()) return;

    const Count old_index = Index;
    Index = new_index;

    c.SetStore(Records[Index].Store);
    const auto direction = new_index > old_index ? Forward : Reverse;
    auto i = int(old_index);
    while (i != int(new_index)) {
        const int history_index = direction == Reverse ? --i : i++;
        const Count record_index = history_index == -1 ? Index : history_index;
        const auto &segment_patch = CreatePatch(Records[record_index].Store, Records[record_index + 1].Store);
        const auto &gesture_time = Records[record_index + 1].Gesture.back().second;
        for (const auto &[partial_path, op] : segment_patch.Ops) {
            const auto &path = segment_patch.BasePath / partial_path;
            if (direction == Forward) {
                CommittedUpdateTimesForPath[path].emplace_back(gesture_time);
            } else if (CommittedUpdateTimesForPath.contains(path)) {
                auto &update_times = CommittedUpdateTimesForPath.at(path);
                update_times.pop_back();
                if (update_times.empty()) CommittedUpdateTimesForPath.erase(path);
            }
        }
    }
    GestureUpdateTimesForPath.clear();
}

//-----------------------------------------------------------------------------
// [SECTION] Action queueing
//-----------------------------------------------------------------------------

static moodycamel::BlockingConcurrentQueue<ActionMoment> ActionQueue; // NOLINT(cppcoreguidelines-interfaces-global-init)

void Context::RunQueuedActions(bool force_finalize_gesture) {
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
            [&](const ProjectAction &a) { ApplyAction(a); },
            [&](const StateAction &a) {
                s.Update(a, transient);
                state_actions.emplace_back(a, action_moment.second);
            }
        );
    }

    const bool finalize = force_finalize_gesture || (!UiContext.IsWidgetGesturing && !History.ActiveGesture.empty() && History.GestureTimeRemainingSec() <= 0);
    if (!state_actions.empty()) {
        History.ActiveGesture.insert(History.ActiveGesture.end(), state_actions.begin(), state_actions.end());
        History.UpdateGesturePaths(state_actions, SetStore(transient.persistent()));
    }
    if (finalize) History.FinalizeGesture();
}

bool q(Action &&action, bool flush) {
    ActionQueue.enqueue({action, Clock::now()});
    if (flush) c.RunQueuedActions(true); // ... unless the `flush` flag is provided, in which case we just finalize the gesture now.
    return true;
}

bool ActionAllowed(ID id) { return c.ActionAllowed(id); }
bool ActionAllowed(const Action &action) { return c.ActionAllowed(action); }
bool ActionAllowed(const EmptyAction &action) { return c.ActionAllowed(action); }
