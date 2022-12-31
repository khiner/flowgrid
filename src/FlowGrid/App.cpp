#include "StateJson.h"

#include "ImGuiFileDialog.h"
#include "blockingconcurrentqueue.h"
#include <immer/algorithm.hpp>
#include <range/v3/view/concat.hpp>
#include <range/v3/view/filter.hpp>

#include "UI/Faust/FaustGraph.h"

map<ID, StateMember *> StateMember::WithId{};
map<StatePath, Base *> Base::WithPath{};
vector<ImVec4> fg::Style::ImGuiStyle::ColorPresetBuffer(ImGuiCol_COUNT);
vector<ImVec4> fg::Style::ImPlotStyle::ColorPresetBuffer(ImPlotCol_COUNT);

// Transient modifiers
void Set(const Base &field, const Primitive &value, TransientStore &store) { store.set(field.Path, value); }
void Set(const StoreEntries &values, TransientStore &store) {
    for (const auto &[path, value] : values) store.set(path, value);
}
void Set(const FieldEntries &values, TransientStore &store) {
    for (const auto &[field, value] : values) store.set(field.Path, value);
}

StateMember::StateMember(StateMember *parent, string_view path_segment, pair<string_view, string_view> name_help)
    : Parent(parent),
      PathSegment(path_segment),
      Path(Parent && !PathSegment.empty() ? Parent->Path / PathSegment : (Parent ? Parent->Path : (!PathSegment.empty() ? StatePath(PathSegment) : RootPath))),
      Name(name_help.first.empty() ? PathSegment.empty() ? "" : PascalToSentenceCase(PathSegment) : name_help.first),
      Help(name_help.second),
      ImGuiLabel(Name.empty() ? "" : format("{}##{}", Name, PathSegment)),
      Id(ImHashStr(ImGuiLabel.c_str(), 0, Parent ? Parent->Id : 0)) {
    if (parent) parent->Children.emplace_back(this);
    WithId[Id] = this;
}

StateMember::StateMember(StateMember *parent, string_view path_segment, string_view name_help) : StateMember(parent, path_segment, ParseHelpText(name_help)) {}

StateMember::~StateMember() {
    WithId.erase(Id);
}

Vec2Linked::Vec2Linked(StateMember *parent, string_view path_segment, string_view name_help, const ImVec2 &value, float min, float max, bool linked, const char *fmt)
    : Vec2(parent, path_segment, name_help, value, min, max, fmt) {
    Set(Linked, linked, c.InitStore);
}

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

string to_string(const IO io, const bool shorten) {
    switch (io) {
        case IO_In: return shorten ? "in" : "input";
        case IO_Out: return shorten ? "out" : "output";
        case IO_None: return "none";
    }
}
string to_string(PatchOp::Type patch_op_type) {
    switch (patch_op_type) {
        case AddOp: return "Add";
        case RemoveOp: return "Remove";
        case ReplaceOp: return "Replace";
    }
}
string to_string(const Primitive &primitive) { return json(primitive).dump(); }

namespace action {

string GetName(const ProjectAction &action) {
    return Match(
        action,
        [](const Undo &) { return ActionName(Undo); },
        [](const Redo &) { return ActionName(Redo); },
        [](const SetHistoryIndex &) { return ActionName(SetHistoryIndex); },
        [](const OpenProject &) { return ActionName(OpenProject); },
        [](const OpenEmptyProject &) { return ActionName(OpenEmptyProject); },
        [](const OpenDefaultProject &) { return ActionName(OpenDefaultProject); },
        [](const SaveProject &) { return ActionName(SaveProject); },
        [](const SaveDefaultProject &) { return ActionName(SaveDefaultProject); },
        [](const SaveCurrentProject &) { return ActionName(SaveCurrentProject); },
        [](const SaveFaustFile &) { return "Save Faust file"s; },
        [](const SaveFaustSvgFile &) { return "Save Faust SVG file"s; },
    );
}

string GetName(const StateAction &action) {
    return Match(
        action,
        [](const OpenFaustFile &) { return "Open Faust file"s; },
        [](const ShowOpenFaustFileDialog &) { return "Show open Faust file dialog"s; },
        [](const ShowSaveFaustFileDialog &) { return "Show save Faust file dialog"s; },
        [](const ShowSaveFaustSvgFileDialog &) { return "Show save Faust SVG file dialog"s; },
        [](const SetImGuiColorStyle &) { return "Set ImGui color style"s; },
        [](const SetImPlotColorStyle &) { return "Set ImPlot color style"s; },
        [](const SetFlowGridColorStyle &) { return "Set FlowGrid color style"s; },
        [](const SetGraphColorStyle &) { return "Set FlowGrid graph color style"s; },
        [](const SetGraphLayoutStyle &) { return "Set FlowGrid graph layout style"s; },
        [](const OpenFileDialog &) { return ActionName(OpenFileDialog); },
        [](const CloseFileDialog &) { return ActionName(CloseFileDialog); },
        [](const ShowOpenProjectDialog &) { return ActionName(ShowOpenProjectDialog); },
        [](const ShowSaveProjectDialog &) { return ActionName(ShowSaveProjectDialog); },
        [](const SetValue &) { return ActionName(SetValue); },
        [](const SetValues &) { return ActionName(SetValues); },
        [](const ToggleValue &) { return ActionName(ToggleValue); },
        [](const ApplyPatch &) { return ActionName(ApplyPatch); },
        [](const CloseApplication &) { return ActionName(CloseApplication); },
    );
}

string GetShortcut(const EmptyAction &action) {
    const ID id = std::visit([](const Action &&a) { return GetId(a); }, action);
    return ShortcutForId.contains(id) ? ShortcutForId.at(id) : "";
}

string GetMenuLabel(const EmptyAction &action) {
    // An action's menu label is its name, except for a few exceptions.
    return Match(
        action,
        [](const ShowOpenProjectDialog &) { return "Open project"s; },
        [](const OpenEmptyProject &) { return "New project"s; },
        [](const SaveCurrentProject &) { return "Save project"s; },
        [](const ShowSaveProjectDialog &) { return "Save project as..."s; },
        [](const ShowOpenFaustFileDialog &) { return "Open DSP file"s; },
        [](const ShowSaveFaustFileDialog &) { return "Save DSP as..."s; },
        [](const ShowSaveFaustSvgFileDialog &) { return "Export SVG"s; },
        [](const ProjectAction &a) { return GetName(a); },
        [](const StateAction &a) { return GetName(a); },
    );
}
} // namespace action

ImGuiTableFlags TableFlagsToImgui(const TableFlags flags) {
    ImGuiTableFlags imgui_flags = ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_SizingStretchProp;
    if (flags & TableFlags_Resizable) imgui_flags |= ImGuiTableFlags_Resizable;
    if (flags & TableFlags_Reorderable) imgui_flags |= ImGuiTableFlags_Reorderable;
    if (flags & TableFlags_Hideable) imgui_flags |= ImGuiTableFlags_Hideable;
    if (flags & TableFlags_Sortable) imgui_flags |= ImGuiTableFlags_Sortable;
    if (flags & TableFlags_ContextMenuInBody) imgui_flags |= ImGuiTableFlags_ContextMenuInBody;
    if (flags & TableFlags_BordersInnerH) imgui_flags |= ImGuiTableFlags_BordersInnerH;
    if (flags & TableFlags_BordersOuterH) imgui_flags |= ImGuiTableFlags_BordersOuterH;
    if (flags & TableFlags_BordersInnerV) imgui_flags |= ImGuiTableFlags_BordersInnerV;
    if (flags & TableFlags_BordersOuterV) imgui_flags |= ImGuiTableFlags_BordersOuterV;
    if (flags & TableFlags_NoBordersInBody) imgui_flags |= ImGuiTableFlags_NoBordersInBody;
    if (flags & TableFlags_PadOuterX) imgui_flags |= ImGuiTableFlags_PadOuterX;
    if (flags & TableFlags_NoPadOuterX) imgui_flags |= ImGuiTableFlags_NoPadOuterX;
    if (flags & TableFlags_NoPadInnerX) imgui_flags |= ImGuiTableFlags_NoPadInnerX;

    return imgui_flags;
}

void State::Update(const StateAction &action, TransientStore &store) const {
    Match(
        action,
        [&store](const SetValue &a) { store.set(a.path, a.value); },
        [&store](const SetValues &a) { ::Set(a.values, store); },
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
        [&](const CloseApplication &) { Set({{UiProcess.Running, false}, {Audio.Running, false}}, store); },
    );
}

Patch CreatePatch(const Store &before, const Store &after, const StatePath &BasePath) {
    PatchOps ops{};
    diff(
        before,
        after,
        [&](auto const &added_element) {
            ops[added_element.first.lexically_relative(BasePath)] = {AddOp, added_element.second, {}};
        },
        [&](auto const &removed_element) {
            ops[removed_element.first.lexically_relative(BasePath)] = {RemoveOp, {}, removed_element.second};
        },
        [&](auto const &old_element, auto const &new_element) {
            ops[old_element.first.lexically_relative(BasePath)] = {ReplaceOp, new_element.second, old_element.second};
        }
    );

    return {ops, BasePath};
}

//-----------------------------------------------------------------------------
// [SECTION] Context
//-----------------------------------------------------------------------------

Context::Context() {
    InitStore = {}; // Transient store only used for `State` construction, so we can clear it to save memory.
    if (fs::exists(PreferencesPath)) {
        Preferences = json::parse(FileIO::read(PreferencesPath));
    } else {
        WritePreferences();
    }
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

bool Context::ClearPreferences() {
    Preferences.RecentlyOpenedPaths.clear();
    return WritePreferences();
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
    if (patch.empty()) return {};

    ApplicationStore = store; // This is the only place `ApplicationStore` is modified.
    for (const auto &[partial_path, _op] : patch.Ops) {
        const auto &path = patch.BasePath / partial_path;
        // todo pretty sure this only happens in the vector case, but we should implement value caching for vectors too!
        if (Base::WithPath.contains(path)) Base::WithPath.at(path)->Update();
        // Setting `ImGuiSettings` does not require a `s.Apply` on the action, since the action will be initiated by ImGui itself,
        // whereas the style editors don't update the ImGui/ImPlot contexts themselves.
        if (path.string().rfind(s.ImGuiSettings.Path.string(), 0) == 0) UiContext.ApplyFlags |= UIContext::Flags_ImGuiSettings; // TODO only when not ui-initiated
        else if (path.string().rfind(s.Style.ImGui.Path.string(), 0) == 0) UiContext.ApplyFlags |= UIContext::Flags_ImGuiStyle;
        else if (path.string().rfind(s.Style.ImPlot.Path.string(), 0) == 0) UiContext.ApplyFlags |= UIContext::Flags_ImPlotStyle;
    }
    s.Audio.UpdateProcess();
    History.LatestUpdatedPaths = patch.Ops | transform([&patch](const auto &entry) { return patch.BasePath / entry.first; }) | to<vector>;
    ProjectHasChanges = true;

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
    Preferences.RecentlyOpenedPaths.remove(path);
    Preferences.RecentlyOpenedPaths.emplace_front(path);
    WritePreferences();
}

bool Context::WritePreferences() const { return FileIO::write(PreferencesPath, json(Preferences).dump()); }

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
    return max(0.f, s.ApplicationSettings.GestureDurationSec - fsec(Clock::now() - GestureStartTime()).count());
}

void StoreHistory::FinalizeGesture() {
    if (ActiveGesture.empty()) return;

    const auto merged_gesture = action::MergeGesture(ActiveGesture);
    ActiveGesture.clear();
    GestureUpdateTimesForPath.clear();
    if (merged_gesture.empty()) return;

    const auto &patch = CreatePatch(AppStore, Records[Index].Store);
    if (patch.empty()) return;

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
    const set<StatePath> paths = views::concat(views::keys(CommittedUpdateTimesForPath), views::keys(GestureUpdateTimesForPath)) | to<set>;
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
