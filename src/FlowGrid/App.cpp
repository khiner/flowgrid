#include "StateJson.h"

#include "blockingconcurrentqueue.h"
#include <range/v3/view/filter.hpp>
#include <range/v3/view/concat.hpp>
#include <immer/algorithm.hpp>
#include "ImGuiFileDialog.h"

map<ID, StateMember *> StateMember::WithId{};

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

// Transient modifiers
void set(const StateMember &member, const Primitive &value, TransientStore &_store) { _store.set(member.Path, value); }
void set(const StoreEntries &values, TransientStore &_store) {
    for (const auto &[path, value]: values) _store.set(path, value);
}
void set(const MemberEntries &values, TransientStore &_store) {
    for (const auto &[member, value]: values) _store.set(member.Path, value);
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
    Id = ImHashStr(Name.c_str(), 0, Parent ? Parent->Id : 0);
    WithId[Id] = this;
}

StateMember::StateMember(const StateMember *parent, const string &id, const Primitive &value) : StateMember(parent, id) {
    c.CtorStore.set(Path, value);
}

StateMember::~StateMember() {
    WithId.erase(Id);
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
    for (const auto &[path, value]: entries) _store.set(path, value);
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

string GetName(const ProjectAction &action) {
    return std::visit(visitor{
        [&](const undo &) { return ActionName(undo); },
        [&](const redo &) { return ActionName(redo); },
        [&](const set_history_index &) { return ActionName(set_history_index); },
        [&](const open_project &) { return ActionName(open_project); },
        [&](const open_empty_project &) { return ActionName(open_empty_project); },
        [&](const open_default_project &) { return ActionName(open_default_project); },
        [&](const save_project &) { return ActionName(save_project); },
        [&](const save_default_project &) { return ActionName(save_default_project); },
        [&](const save_current_project &) { return ActionName(save_current_project); },
        [&](const save_faust_file &) { return "Save Faust file"s; },
        [&](const save_faust_svg_file &) { return "Save Faust SVG file"s; },
    }, action);
}

string GetName(const StateAction &action) {
    return std::visit(visitor{
        [&](const open_faust_file &) { return "Open Faust file"s; },
        [&](const show_open_faust_file_dialog &) { return "Show open Faust file dialog"s; },
        [&](const show_save_faust_file_dialog &) { return "Show save Faust file dialog"s; },
        [&](const show_save_faust_svg_file_dialog &) { return "Show save Faust SVG file dialog"s; },
        [&](const set_imgui_color_style &) { return "Set ImGui color style"s; },
        [&](const set_implot_color_style &) { return "Set ImPlot color style"s; },
        [&](const set_flowgrid_color_style &) { return "Set FlowGrid color style"s; },
        [&](const set_flowgrid_diagram_color_style &) { return "Set FlowGrid diagram color style"s; },
        [&](const set_flowgrid_diagram_layout_style &) { return "Set FlowGrid diagram layout style"s; },
        [&](const open_file_dialog &) { return ActionName(open_file_dialog); },
        [&](const close_file_dialog &) { return ActionName(close_file_dialog); },
        [&](const show_open_project_dialog &) { return ActionName(show_open_project_dialog); },
        [&](const show_save_project_dialog &) { return ActionName(show_save_project_dialog); },
        [&](const set_value &) { return ActionName(set_value); },
        [&](const set_values &) { return ActionName(set_values); },
        [&](const toggle_value &) { return ActionName(toggle_value); },
        [&](const apply_patch &) { return ActionName(apply_patch); },
        [&](const close_application &) { return ActionName(close_application); },
    }, action);
}

string GetShortcut(const EmptyAction &action) {
    const ID id = std::visit(visitor{[&](const Action &a) { return GetId(a); }}, action);
    return ShortcutForId.contains(id) ? ShortcutForId.at(id) : "";
}

string GetMenuLabel(const EmptyAction &action) {
    // An action's menu label is its name, except for a few exceptions.
    return std::visit(visitor{
        [&](const show_open_project_dialog &) { return "Open project"s; },
        [&](const open_empty_project &) { return "New project"s; },
        [&](const save_current_project &) { return "Save project"s; },
        [&](const show_save_project_dialog &) { return "Save project as..."s; },
        [&](const show_open_faust_file_dialog &) { return "Open DSP file"s; },
        [&](const show_save_faust_file_dialog &) { return "Save DSP as..."s; },
        [&](const show_save_faust_svg_file_dialog &) { return "Export SVG"s; },
        [&](const ProjectAction &a) { return GetName(a); },
        [&](const StateAction &a) { return GetName(a); },
    }, action);
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

void State::Update(const StateAction &action, TransientStore &transient) const {
    std::visit(visitor{
        [&](const set_value &a) { transient.set(a.path, a.value); },
        [&](const set_values &a) { ::set(a.values, transient); },
        [&](const toggle_value &a) { transient.set(a.path, !std::get<bool>(store.at(a.path))); },
        [&](const apply_patch &a) {
            const auto &patch = a.patch;
            for (const auto &[partial_path, op]: patch.Ops) {
                const auto &path = patch.BasePath / partial_path;
                if (op.Op == Add || op.Op == Replace) transient.set(path, *op.Value);
                else if (op.Op == Remove) transient.erase(path);
            }
        },
        [&](const open_file_dialog &a) { FileDialog.set(json::parse(a.dialog_json), transient); },
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
        [&](const close_application &) { set({{UiProcess.Running, false}, {Audio.Running, false}}, transient); },
    }, action);
}

Patch CreatePatch(const Store &before, const Store &after, const StatePath &BasePath) {
    PatchOps ops{};
    diff(
        before,
        after,
        [&](auto const &added_element) {
            ops[added_element.first.lexically_relative(BasePath)] = {Add, added_element.second, {}};
        },
        [&](auto const &removed_element) {
            ops[removed_element.first.lexically_relative(BasePath)] = {Remove, {}, removed_element.second};
        },
        [&](auto const &old_element, auto const &new_element) {
            ops[old_element.first.lexically_relative(BasePath)] = {Replace, new_element.second, old_element.second};
        });

    return {ops, BasePath};
}

void SaveBoxSvg(const string &path); // Defined in FaustUI

//-----------------------------------------------------------------------------
// [SECTION] Context
//-----------------------------------------------------------------------------

Context::Context() {
    CtorStore = {}; // Transient store only used for `State` construction, so we can clear it to save memory.
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
void Context::SaveCurrentProject() { if (CurrentProjectPath) SaveProject(*CurrentProjectPath); }

bool Context::ClearPreferences() {
    Preferences.RecentlyOpenedPaths.clear();
    return WritePreferences();
}

json Context::GetProjectJson(const ProjectFormat format) {
    switch (format) {
        case StateFormat: return store;
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

Patch Context::SetStore(const Store &new_store) {
    const auto &patch = CreatePatch(store, new_store);
    if (patch.empty()) return {};

    ApplicationStore = new_store; // This is the only place `ApplicationStore` is modified.
    for (const auto &[partial_path, _op]: patch.Ops) {
        const auto &path = patch.BasePath / partial_path;
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
        auto transient = store.transient();
        for (const auto &gesture: gestures) {
            const auto before_store = transient.persistent();
            for (const auto &action_moment: gesture) {
                s.Update(action_moment.first, transient);
            }
            const auto after_store = transient.persistent();
            const auto &patch = CreatePatch(before_store, after_store);
            const auto &gesture_time = gesture.back().second;
            History.Records.push_back({gesture_time, after_store, gesture}); // todo save/load gesture commit times
            History.Index = History.Size() - 1;
            for (const auto &[partial_path, op]: patch.Ops) History.CommittedUpdateTimesForPath[patch.BasePath / partial_path].emplace_back(gesture_time);
        }
        SetStore(transient.persistent());
        History.SetIndex(project["index"]);
    }

    if (IsUserProjectPath(path)) SetCurrentProjectPath(path);
}

bool Context::SaveProject(const fs::path &path) {
    const bool is_current_project = CurrentProjectPath && fs::equivalent(path, *CurrentProjectPath);
    if (is_current_project && !ActionAllowed(action::id<save_current_project>)) return false;

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
        case action::id<undo>: return History.CanUndo();
        case action::id<redo>: return History.CanRedo();
        case action::id<open_default_project>: return fs::exists(DefaultProjectPath);
        case action::id<save_project>:
        case action::id<save_default_project>: return !History.Empty();
        case action::id<show_save_project_dialog>:
            // If there is no current project, `save_current_project` will be transformed into a `show_save_project_dialog`.
        case action::id<save_current_project>: return ProjectHasChanges;
        case action::id<open_file_dialog>: return !s.FileDialog.Visible;
        case action::id<close_file_dialog>: return s.FileDialog.Visible;
        default: return true;
    }
}
bool Context::ActionAllowed(const Action &action) const { return ActionAllowed(action::GetId(action)); }
bool Context::ActionAllowed(const EmptyAction &action) const { return std::visit(visitor{[&](const Action &a) { return ActionAllowed(a); }}, action); }

void Context::ApplyAction(const ProjectAction &action) {
    std::visit(visitor{
        // Handle actions that don't directly update state.
        // These options don't get added to the action/gesture history, since they only have non-application side effects,
        // and we don't want them replayed when loading a saved `.fga` project.
        [&](const open_project &a) { OpenProject(a.path); },
        [&](const open_empty_project &) { OpenProject(EmptyProjectPath); },
        [&](const open_default_project &) { OpenProject(DefaultProjectPath); },

        [&](const save_project &a) { SaveProject(a.path); },
        [&](const save_default_project &) { SaveProject(DefaultProjectPath); },
        [&](const save_current_project &) { SaveCurrentProject(); },
        [&](const save_faust_file &a) { FileIO::write(a.path, s.Audio.Faust.Code); },
        [&](const save_faust_svg_file &a) { SaveBoxSvg(a.path); },

        // `History.Index`-changing actions:
        [&](const undo &) {
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
        [&](const redo &) { History.SetIndex(History.Index + 1); },
        [&](const set_history_index &a) { History.SetIndex(a.index); },
    }, action);
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

    const auto &patch = CreatePatch(store, Records[Index].Store);
    if (patch.empty()) return;

    while (Size() > Index + 1) Records.pop_back(); // TODO use an undo _tree_ and keep this history
    Records.push_back({Clock::now(), store, merged_gesture});
    Index = Size() - 1;
    const auto &gesture_time = merged_gesture.back().second;
    for (const auto &[partial_path, op]: patch.Ops) CommittedUpdateTimesForPath[patch.BasePath / partial_path].emplace_back(gesture_time);
}

void StoreHistory::UpdateGesturePaths(const Gesture &gesture, const Patch &patch) {
    const auto &gesture_time = gesture.back().second;
    for (const auto &[partial_path, op]: patch.Ops) GestureUpdateTimesForPath[patch.BasePath / partial_path].emplace_back(gesture_time);
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
    Count i = 0;
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

void StoreHistory::SetIndex(Count new_index) {
    // If we're mid-gesture, revert the current gesture before navigating to the requested history index.
    if (!ActiveGesture.empty()) {
        ActiveGesture.clear();
        GestureUpdateTimesForPath.clear();
        c.SetStore(Records[Index].Store);
    }
    if (new_index == Index || new_index < 0 || new_index >= Size()) return;

    Count old_index = Index;
    Index = new_index;

    c.SetStore(Records[Index].Store);
    const auto direction = new_index > old_index ? Forward : Reverse;
    auto i = int(old_index);
    while (i != int(new_index)) {
        const int history_index = direction == Reverse ? --i : i++;
        const Count record_index = history_index == -1 ? Index : history_index;
        const auto &segment_patch = CreatePatch(Records[record_index].Store, Records[record_index + 1].Store);
        const auto &gesture_time = Records[record_index + 1].Gesture.back().second;
        for (const auto &[partial_path, op]: segment_patch.Ops) {
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

    auto transient = store.transient();
    while (ActionQueue.try_dequeue(action_moment)) {
        // Note that multiple actions enqueued during the same frame (in the same queue batch) are all evaluated independently to see if they're allowed.
        // This means that if one action would change the state such that a later action in the same batch _would be allowed_,
        // the current approach would incorrectly throw this later action away.
        auto &[action, _] = action_moment;
        if (!ActionAllowed(action)) continue;

        // Special cases:
        // * If saving the current project where there is none, open the save project dialog so the user can tell us where to save it:
        if (std::holds_alternative<save_current_project>(action) && !CurrentProjectPath) action = show_save_project_dialog{};
        // * Treat all toggles as immediate actions. Otherwise, performing two toggles in a row compresses into nothing:
        force_finalize_gesture |= std::holds_alternative<toggle_value>(action);

        std::visit(visitor{
            [&](const ProjectAction &a) { ApplyAction(a); },
            [&](const StateAction &a) {
                s.Update(a, transient);
                state_actions.emplace_back(a, action_moment.second);
            },
        }, action);
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
