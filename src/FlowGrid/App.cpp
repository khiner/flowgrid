#include "StateJson.h"

#include "blockingconcurrentqueue.h"
#include <range/v3/view/filter.hpp>
#include <range/v3/view/concat.hpp>
#include <immer/algorithm.hpp>
#include "ImGuiFileDialog.h"

using namespace moodycamel; // Has `ConcurrentQueue` & `BlockingConcurrentQueue`

map<ImGuiID, StateMember *> StateMember::WithID{};

std::queue<const ActionMoment> ActionQueue;
BlockingConcurrentQueue<ActionMoment> ActionConcurrentQueue{}; // NOLINT(cppcoreguidelines-interfaces-global-init)

// Persistent modifiers
Store set(const StateMember &member, const Primitive &value, const Store &_store) { return _store.set(member.Path, value); }
Store set(const StoreEntries &values, const Store &_store) {
    auto transient = _store.transient();
    set(values, transient);
    return transient.persistent();
}
Store set(const MemberEntries &values, const Store &_store) {
    auto transient = _store.transient();
    set(values, transient);
    return transient.persistent();
}
Store set(const std::vector<std::pair<StatePath, ImVec4>> &values, const Store &_store) {
    auto transient = _store.transient();
    set(values, transient);
    return transient.persistent();
}

// Transient modifiers
void set(const StateMember &member, const Primitive &value, TransientStore &_store) { _store.set(member.Path, value); }
void set(const StoreEntries &values, TransientStore &_store) {
    for (const auto &[path, value]: values) _store.set(path, value);
}
void set(const MemberEntries &values, TransientStore &_store) {
    for (const auto &[member, value]: values) _store.set(member.Path, value);
}
void set(const std::vector<std::pair<StatePath, ImVec4>> &values, TransientStore &_store) {
    for (const auto &[path, value]: values) _store.set(path, value);
}

// Split the string on '#'.
// If there is no '#' in the provided string, the first element will have the full input string and the second element will be an empty string.
// todo don't split on escaped '\#'
static std::pair<string, string> parse_name(const string &str) {
    const auto help_split = str.find_first_of('#');
    const bool found = help_split != string::npos;
    return {found ? str.substr(0, help_split) : str, found ? str.substr(help_split + 1) : ""};
}

StateMember::StateMember(const StateMember *parent, const string &id) : Parent(parent) {
    const auto &[path_segment_and_name, help] = parse_help_text(id);
    const auto &[path_segment, name] = parse_name(path_segment_and_name);
    PathSegment = path_segment;
    Path = Parent && !PathSegment.empty() ? Parent->Path / PathSegment : Parent ? Parent->Path : !PathSegment.empty() ? StatePath(PathSegment) : RootPath;
    Name = name.empty() ? path_segment.empty() ? "" : SnakeCaseToSentenceCase(path_segment) : name;
    Help = help;
    ImGuiId = ImHashStr(Name.c_str(), 0, Parent ? Parent->ImGuiId : 0);
    WithID[ImGuiId] = this;
}

StateMember::StateMember(const StateMember *parent, const string &id, const Primitive &value) : StateMember(parent, id) {
    ctor_store.set(Path, value);
}

StateMember::~StateMember() {
    WithID.erase(ImGuiId);
}

namespace nlohmann {
inline void to_json(json &j, const Store &v) {
    for (const auto &[key, value]: v) j[json::json_pointer(key.string())] = value;
}
}

// `from_json` defined out of `nlohmann`, to be called manually.
// This avoids getting a reference arg to a default-constructed, non-transient `Store` instance.
Store store_from_json(const json &j) {
    const auto &flattened = j.flatten();
    StoreEntries entries(flattened.size());
    int item_index = 0;
    for (const auto &[key, value]: flattened.items()) entries[item_index++] = {StatePath(key), Primitive(value)};

    TransientStore _store;
    for (size_t i = 0; i < entries.size(); i++) {
        const auto &[path, value] = entries[i];
        if (path.filename() == "w" && i < entries.size() - 3 && entries[i + 3].first.filename() == "z") {
            const auto w = std::get<float>(value);
            const auto x = std::get<float>(entries[i + 1].second);
            const auto y = std::get<float>(entries[i + 2].second);
            const auto z = std::get<float>(entries[i + 3].second);
            _store.set(path.parent_path(), ImVec4{x, y, z, w});
            i += 3;
        } else if (path.filename() == "x" && i < entries.size() - 1 && entries[i + 1].first.filename() == "y") {
            if (std::holds_alternative<unsigned int>(value)) {
                const auto x = std::get<unsigned int>(value);
                const auto y = std::get<unsigned int>(entries[i + 1].second);
                _store.set(path.parent_path(), ImVec2ih{short(x), short(y)});
            } else if (std::holds_alternative<int>(value)) {
                const auto x = std::get<int>(value);
                const auto y = std::get<int>(entries[i + 1].second);
                _store.set(path.parent_path(), ImVec2ih{short(x), short(y)});
            } else {
                const auto x = std::get<float>(value);
                const auto y = std::get<float>(entries[i + 1].second);
                _store.set(path.parent_path(), ImVec2{x, y});
            }
            i += 1;
        } else {
            _store.set(path, value);
        }
    }
    return _store.persistent();
}

string to_string(const IO io, const bool shorten) {
    switch (io) {
        case IO_In: return shorten ? "in" : "input";
        case IO_Out: return shorten ? "out" : "output";
        case IO_None: return "none";
    }
}
string to_string(PatchOpType type) {
    switch (type) {
        case Add: return "Add";
        case Remove: return "Remove";
        case Replace: return "Replace";
    }
}
string to_string(const Primitive &primitive) { return json(primitive).dump(); }

namespace action {


// An action's menu label is its name, except for a few exceptions.
const map<ID, string> menu_label_for_id{
    {id<show_open_project_dialog>, "Open project"},
    {id<open_empty_project>, "New project"},
    {id<save_current_project>, "Save project"},
    {id<show_save_project_dialog>, "Save project as..."},
    {id<show_open_faust_file_dialog>, "Open DSP file"},
    {id<show_save_faust_file_dialog>, "Save DSP as..."},
    {id<show_save_faust_svg_file_dialog>, "Export SVG"},
};
string GetName(const Action &action) { return NameForId.at(GetId(action)); }
const char *GetMenuLabel(ID action_id) {
    if (menu_label_for_id.contains(action_id)) return menu_label_for_id.at(action_id).c_str();
    return NameForId.at(action_id).c_str();
}
}

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

void State::Update(const Action &action, TransientStore &transient) const {
    std::visit(visitor{
        [&](const set_value &a) { transient.set(a.path, a.value); },
        [&](const set_values &a) { ::set(a.values, transient); },
        [&](const toggle_value &a) { transient.set(a.path, !std::get<bool>(store.at(a.path))); },
        [&](const apply_patch &a) {
            const auto &patch = a.patch;
            for (const auto &[partial_path, op]: patch.ops) {
                const auto &path = patch.base_path / partial_path;
                if (op.op == Add || op.op == Replace) transient.set(path, op.value.value());
                else if (op.op == Remove) transient.erase(path);
            }
        },
        [&](const open_file_dialog &a) { FileDialog.set(json(a.dialog_json), transient); },
        [&](const close_file_dialog &) { set(FileDialog.Visible, false, transient); },
        [&](const show_open_project_dialog &) { FileDialog.set({"Choose file", AllProjectExtensionsDelimited, ".", ""}, transient); },
        [&](const show_save_project_dialog &) { FileDialog.set({"Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}, transient); },
        [&](const show_open_faust_file_dialog &) { FileDialog.set({"Choose file", FaustDspFileExtension, ".", ""}, transient); },
        [&](const show_save_faust_file_dialog &) { FileDialog.set({"Choose file", FaustDspFileExtension, ".", "my_dsp", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}, transient); },
        [&](const show_save_faust_svg_file_dialog &) { FileDialog.set({"Choose directory", ".*", ".", "faust_diagram", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}, transient); },

        [&](const set_imgui_color_style &a) {
            switch (a.id) {
                case 0: return Style.ImGui.ColorsDark(transient);
                case 1: return Style.ImGui.ColorsLight(transient);
                case 2: return Style.ImGui.ColorsClassic(transient);
            }
        },
        [&](const set_implot_color_style &a) {
            switch (a.id) {
                case 0: return Style.ImPlot.ColorsAuto(transient);
                case 1: return Style.ImPlot.ColorsDark(transient);
                case 2: return Style.ImPlot.ColorsLight(transient);
                case 3: return Style.ImPlot.ColorsClassic(transient);
            }
        },
        [&](const set_flowgrid_color_style &a) {
            switch (a.id) {
                case 0: return Style.FlowGrid.ColorsDark(transient);
                case 1: return Style.FlowGrid.ColorsLight(transient);
                case 2: return Style.FlowGrid.ColorsClassic(transient);
            }
        },
        [&](const set_flowgrid_diagram_color_style &a) {
            switch (a.id) {
                case 0: return Style.FlowGrid.DiagramColorsDark(transient);
                case 1: return Style.FlowGrid.DiagramColorsLight(transient);
                case 2: return Style.FlowGrid.DiagramColorsClassic(transient);
                case 3: return Style.FlowGrid.DiagramColorsFaust(transient);
            }
        },
        [&](const set_flowgrid_diagram_layout_style &a) {
            switch (a.id) {
                case 0: return Style.FlowGrid.DiagramLayoutFlowGrid(transient);
                case 1: return Style.FlowGrid.DiagramLayoutFaust(transient);
            }
        },
        [&](const open_faust_file &a) { set(Audio.Faust.Code, FileIO::read(a.path), transient); },
        [&](const close_application &) { set({{UiProcess.Running, false}, {Audio.Running, false}, {ApplicationSettings.ActionConsumer.Running, false}}, transient); },
        [&](const auto &) {}, // All actions that don't directly update state (undo/redo & open/load-project, etc.)
    }, action);
}

Patch CreatePatch(const Store &before, const Store &after, const StatePath &base_path) {
    PatchOps ops{};
    diff(
        before,
        after,
        [&](auto const &added_element) {
            ops[added_element.first.lexically_relative(base_path)] = {Add, added_element.second, {}};
        },
        [&](auto const &removed_element) {
            ops[removed_element.first.lexically_relative(base_path)] = {Remove, {}, removed_element.second};
        },
        [&](auto const &old_element, auto const &new_element) {
            ops[old_element.first.lexically_relative(base_path)] = {Replace, new_element.second, old_element.second};
        });

    return {ops, base_path};
}

void SaveBoxSvg(const string &path); // Defined in FaustUI

//-----------------------------------------------------------------------------
// [SECTION] Context
//-----------------------------------------------------------------------------

Context::Context() {
    History = {};
    if (fs::exists(PreferencesPath)) {
        preferences = json::parse(FileIO::read(PreferencesPath));
    } else {
        WritePreferences();
    }
}

Context::~Context() = default;

bool Context::IsUserProjectPath(const fs::path &path) {
    // Using relative path to avoid error: `filesystem error: in equivalent: Operation not supported`
    return !fs::equivalent(fs::relative(path), EmptyProjectPath) && !fs::equivalent(fs::relative(path), DefaultProjectPath);
}

void Context::SaveEmptyProject() {
    SaveProject(EmptyProjectPath);
    if (!fs::exists(DefaultProjectPath)) SaveProject(DefaultProjectPath);
}

void Context::SaveCurrentProject() {
    if (CurrentProjectPath) SaveProject(CurrentProjectPath.value());
}

bool Context::ClearPreferences() {
    preferences.recently_opened_paths.clear();
    return WritePreferences();
}

json Context::GetProjectJson(const ProjectFormat format) {
    switch (format) {
        case StateFormat: return store;
        case ActionFormat: return {{"gestures", History.Gestures()}, {"index", History.Index}};
    }
}

void Context::RunQueuedActions(bool force_finalize_gesture) {
    vector<ActionMoment> actions; // Same type as `Gesture`, but semantically different, since the queued actions may not represent a full gesture.
    while (!ActionQueue.empty()) {
        actions.push_back(ActionQueue.front());
        ActionQueue.pop();
    }
    ApplyGesture(actions, force_finalize_gesture || (!UiContext.IsWidgetGesturing && !History.ActiveGesture.empty() && History.GestureTimeRemainingSec() <= 0));
}

bool Context::ActionAllowed(const ActionID action_id) const {
    switch (action_id) {
        case action::id<undo>: return History.CanUndo();
        case action::id<redo>: return History.CanRedo();
        case action::id<Actions::open_default_project>: return fs::exists(DefaultProjectPath);
        case action::id<Actions::save_project>:
        case action::id<Actions::show_save_project_dialog>:
        case action::id<Actions::save_default_project>: return !History.Empty();
        case action::id<Actions::save_current_project>: return CurrentProjectPath.has_value() && !History.Empty();
        case action::id<Actions::open_file_dialog>: return !s.FileDialog.Visible;
        case action::id<Actions::close_file_dialog>: return s.FileDialog.Visible;
        default: return true;
    }
}
bool Context::ActionAllowed(const Action &action) const { return ActionAllowed(action::GetId(action)); }

void Context::Clear() {
    CurrentProjectPath = {};
    History = {};
    UiContext.IsWidgetGesturing = false;
}

std::optional<ProjectFormat> GetProjectFormat(const fs::path &path) {
    const string &ext = path.extension();
    if (ProjectFormatForExtension.contains(ext)) return ProjectFormatForExtension.at(ext);
    return {};
}

Patch SetStoreAndNotify(const Store &new_store, const Store &prev_store = store) {
    const auto &patch = CreatePatch(prev_store, new_store);
    if (!patch.empty()) {
        SetStore(new_store); // This is the only place `SetStore` is called.
        UIContext::Flags ui_context_flags = UIContext::Flags_None;
        for (const auto &[partial_path, _op]: patch.ops) {
            const auto &path = patch.base_path / partial_path;
            // Setting `ImGuiSettings` does not require a `s.Apply` on the action, since the action will be initiated by ImGui itself,
            // whereas the style editors don't update the ImGui/ImPlot contexts themselves.
            if (path.string().rfind(s.ImGuiSettings.Path.string(), 0) == 0) ui_context_flags |= UIContext::Flags_ImGuiSettings; // TODO only when not ui-initiated
            else if (path.string().rfind(s.Style.ImGui.Path.string(), 0) == 0) ui_context_flags |= UIContext::Flags_ImGuiStyle;
            else if (path.string().rfind(s.Style.ImPlot.Path.string(), 0) == 0) ui_context_flags |= UIContext::Flags_ImPlotStyle;
        }
        if (ui_context_flags != UIContext::Flags_None) s.Apply(ui_context_flags);
        s.Audio.UpdateProcess();
        s.ApplicationSettings.ActionConsumer.UpdateProcess();
        c.History.LatestUpdatedPaths = patch.ops | transform([&patch](const auto &entry) { return patch.base_path / entry.first; }) | to<vector>;
    }
    return patch;
}

void Context::OpenProject(const fs::path &path) {
    const auto format = GetProjectFormat(path);
    if (!format) return; // TODO log

    Clear();

    const json project = json::parse(FileIO::read(path));
    if (format == StateFormat) {
        SetStoreAndNotify(store_from_json(project));
    } else if (format == ActionFormat) {
        OpenProject(EmptyProjectPath);

        const Gestures gestures = project["gestures"];
        for (const auto &gesture: gestures) ApplyGesture(gesture, true);
        History.SetIndex(project["index"]);
    }

    if (IsUserProjectPath(path)) SetCurrentProjectPath(path);
}

bool Context::SaveProject(const fs::path &path) {
    if (CurrentProjectPath.has_value() && fs::equivalent(path, CurrentProjectPath.value()) && !ActionAllowed(action::id<save_current_project>))
        return false;

    const auto format = GetProjectFormat(path);
    if (!format) return false; // TODO log

    History.FinalizeGesture(); // Make sure any pending actions/diffs are committed.
    if (FileIO::write(path, GetProjectJson(format.value()).dump())) {
        if (IsUserProjectPath(path)) SetCurrentProjectPath(path);
        return true;
    }
    return false;
}

void Context::SetCurrentProjectPath(const fs::path &path) {
    CurrentProjectPath = path;
    preferences.recently_opened_paths.remove(path);
    preferences.recently_opened_paths.emplace_front(path);
    WritePreferences();
}

bool Context::WritePreferences() const {
    return FileIO::write(PreferencesPath, json(preferences).dump());
}

void Context::ApplyAction(const ActionMoment &action_moment, TransientStore &transient) {
    const auto &[action, time] = action_moment;
    if (!ActionAllowed(action)) return; // Safeguard against actions running in an invalid state.

    std::visit(visitor{
        // Handle actions that don't directly update state.
        // These options don't get added to the action/gesture history, since they only have non-application side effects,
        // and we don't want them replayed when loading a saved `.fga` project.
        [&](const Actions::open_project &a) { OpenProject(a.path); },
        [&](const open_empty_project &) { OpenProject(EmptyProjectPath); },
        [&](const open_default_project &) { OpenProject(DefaultProjectPath); },

        [&](const Actions::save_project &a) { SaveProject(a.path); },
        [&](const save_default_project &) { SaveProject(DefaultProjectPath); },
        [&](const Actions::save_current_project &) { SaveCurrentProject(); },
        [&](const save_faust_file &a) { FileIO::write(a.path, s.Audio.Faust.Code); },
        [&](const save_faust_svg_file &a) { SaveBoxSvg(a.path); },

        // `History.Index`-changing actions:
        [&](const undo &) {
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
        [&](const redo &) { History.SetIndex(History.Index + 1); },
        [&](const Actions::set_history_index &a) { History.SetIndex(a.index); },

        // Remaining actions have a direct effect on the application state.
        [&](const auto &a) {
            History.ActiveGesture.emplace_back(action_moment);
            s.Update(a, transient);
        },
    }, action);
}

void Context::ApplyGesture(const Gesture &gesture, const bool force_finalize) {
    bool finalize = force_finalize;
    const auto prev_store = store;
    auto transient = store.transient();
    for (const auto &action_moment: gesture) {
        ApplyAction(action_moment, transient);
        // Treat all toggles as immediate actions. Otherwise, performing two toggles in a row compresses into nothing.
        finalize |= std::holds_alternative<toggle_value>(action_moment.first);
    }
    const auto &patch = SetStoreAndNotify(transient.persistent(), prev_store);
    History.ApplyToCurrentGesture(gesture, patch);
    if (finalize) History.FinalizeGesture();
}

//-----------------------------------------------------------------------------
// [SECTION] History
//-----------------------------------------------------------------------------

int StoreHistory::Size() const { return int(Records.size()); }
bool StoreHistory::Empty() const { return Size() <= 1; } // There is always an initial store in the history records.
bool StoreHistory::CanUndo() const { return !ActiveGesture.empty() || Index > 0; }
bool StoreHistory::CanRedo() const { return Index < Size(); }

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
    return max(0.0f, s.ApplicationSettings.GestureDurationSec - fsec(Clock::now() - GestureStartTime()).count());
}

void StoreHistory::FinalizeGesture() {
    if (ActiveGesture.empty()) return;

    const auto merged_gesture = action::MergeGesture(ActiveGesture);
    ActiveGesture.clear();
    GestureUpdateTimesForPath.clear();
    if (merged_gesture.empty()) return;

    const auto &patch = CreatePatch(store, Records[Index].Store);
    if (patch.empty()) return;

    while (Size() > Index + 1) Records.pop_back(); // TODO use an undo _tree_ and keep this history
    Records.push_back({Clock::now(), store, merged_gesture});
    Index = Size() - 1;
    const auto &gesture_time = merged_gesture.back().second;
    for (const auto &[partial_path, op]: patch.ops) CommittedUpdateTimesForPath[patch.base_path / partial_path].emplace_back(gesture_time);
}

void StoreHistory::ApplyToCurrentGesture(const Gesture &gesture, const Patch &patch) {
    const auto &gesture_time = gesture.back().second;
    for (const auto &[partial_path, op]: patch.ops) GestureUpdateTimesForPath[patch.base_path / partial_path].emplace_back(gesture_time);
}

std::optional<TimePoint> StoreHistory::LatestUpdateTime(const StatePath &path) const {
    if (GestureUpdateTimesForPath.contains(path)) return GestureUpdateTimesForPath.at(path).back();
    if (CommittedUpdateTimesForPath.contains(path)) return CommittedUpdateTimesForPath.at(path).back();
    return {};
}

StoreHistory::Plottable StoreHistory::StatePathUpdateFrequencyPlottable() const {
    std::set < StatePath > paths = views::concat(views::keys(CommittedUpdateTimesForPath), views::keys(GestureUpdateTimesForPath)) | to<std::set>;
    if (paths.empty()) return {};

    const bool has_gesture = !GestureUpdateTimesForPath.empty();
    vector<ImU64> values(has_gesture ? paths.size() * 2 : paths.size());
    int i = 0;
    for (const auto &path: paths) values[i++] = CommittedUpdateTimesForPath.contains(path) ? CommittedUpdateTimesForPath.at(path).size() : 0;
    // Optionally add a second plot item for gesturing update times. See `ImPlot::PlotBarGroups` for value ordering explanation.
    if (has_gesture) for (const auto &path: paths) values[i++] = GestureUpdateTimesForPath.contains(path) ? GestureUpdateTimesForPath.at(path).size() : 0;

    const auto labels = paths | transform([](const string &path) {
        // Convert `string` to char array, removing first character of the path, which is a '/'.
        char *label = new char[path.size()];
        std::strcpy(label, string{path.begin() + 1, path.end()}.c_str());
        return label;
    }) | to<vector<const char *>>;

    return {labels, values};
}

void StoreHistory::SetIndex(int new_index) {
    // If we're mid-gesture, revert the current gesture before navigating to the requested history index.
    if (!ActiveGesture.empty()) {
        ActiveGesture.clear();
        GestureUpdateTimesForPath.clear();
        SetStoreAndNotify(Records[Index].Store);
    }
    if (new_index == Index || new_index < 0 || new_index >= Size()) return;

    int old_index = Index;
    Index = new_index;

    SetStoreAndNotify(Records[Index].Store);
    const auto direction = new_index > old_index ? Forward : Reverse;
    int i = old_index;
    while (i != new_index) {
        const int history_index = direction == Reverse ? --i : i++;
        const size_t record_index = history_index == -1 ? Index : history_index;
        const auto &segment_patch = CreatePatch(Records[record_index].Store, Records[record_index + 1].Store);
        const auto &gesture_time = Records[record_index + 1].Gesture.back().second;
        for (const auto &[partial_path, op]: segment_patch.ops) {
            const auto &path = segment_patch.base_path / partial_path;
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

static std::atomic<bool> ActionConsumerRunning = false; // Thread loop signal

void ConsumeActions() {
    static ActionMoment action_moment;
    while (ActionConsumerRunning) {
        if (ActionConcurrentQueue.try_dequeue(action_moment)) {
            const auto &[action, time] = action_moment;
            cout << format("Consumed action produced at {}:\n{}\n\n", time, json(action).dump(4));
        }
    }
}

void ApplicationSettings::ActionConsumer::UpdateProcess() const {
    static std::thread ActionConsumerThread;
    ActionConsumerRunning = Running;
    if (ActionConsumerRunning && !ActionConsumerThread.joinable()) {
        ActionConsumerThread = std::thread(ConsumeActions);
    } else if (!ActionConsumerRunning && ActionConsumerThread.joinable()) {
        ActionConsumerThread.join();
    }
}

void EnqueueAction(const Action &action) {
    const ActionMoment action_moment = {action, Clock::now()};
    ActionQueue.push(action_moment);
    ActionConcurrentQueue.enqueue(action_moment);
}

bool q(Action &&a, bool flush) {
    EnqueueAction(a); // Actions within a single UI frame are queued up and flushed at the end of the frame.
    if (flush) c.RunQueuedActions(true); // ... unless the `flush` flag is provided, in which case we just finalize the gesture now.
    return true;
}
