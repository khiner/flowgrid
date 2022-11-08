#include "App.h"
#include "StateJson.h"

#include <immer/algorithm.hpp>
#include "ImGuiFileDialog.h"

map<ImGuiID, StateMember *> StateMember::WithID{};

StoreHistory store_history{}; // One store checkpoint for every gesture.
const StoreHistory &history = store_history;

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

StateMember::StateMember(const StateMember *parent, const string &id, const Primitive &value) : StateMember(parent, id) {
    set(store.set(Path, value));
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
string get_name(const Action &action) { return name_for_id.at(get_id(action)); }
const char *get_menu_label(ID action_id) {
    if (menu_label_for_id.contains(action_id)) return menu_label_for_id.at(action_id).c_str();
    return name_for_id.at(action_id).c_str();
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

// todo refactor to always run through this when (and only when) global `set(store)` is called
void on_set_value(const StatePath &path) {
    // Setting `ImGuiSettings` does not require a `s.Apply` on the action, since the action will be initiated by ImGui itself,
    // whereas the style editors don't update the ImGui/ImPlot contexts themselves.
    if (path.string().rfind(s.ImGuiSettings.Path.string(), 0) == 0) s.Apply(UIContext::Flags_ImGuiSettings); // TODO only when not ui-initiated
    else if (path.string().rfind(s.Style.ImGui.Path.string(), 0) == 0) s.Apply(UIContext::Flags_ImGuiStyle);
    else if (path.string().rfind(s.Style.ImPlot.Path.string(), 0) == 0) s.Apply(UIContext::Flags_ImPlotStyle);
}
void on_patch(const Patch &patch) {
    for (const auto &[partial_path, _op]: patch.ops) on_set_value(patch.base_path / partial_path);
    s.Audio.update_process();
    store_history.stats.Apply(patch, Clock::now(), Forward, false);
}

Store State::Update(const Action &action) const {
    return std::visit(visitor{
        [&](const set_value &a) { return store.set(a.path, a.value); },
        [&](const set_values &a) { return ::set(a.values); },
        [&](const toggle_value &a) { return store.set(a.path, !std::get<bool>(store.at(a.path))); },
        [&](const apply_patch &a) {
            const auto &patch = a.patch;
            auto transient = store.transient();
            for (const auto &[partial_path, op]: patch.ops) {
                const auto &path = patch.base_path / partial_path;
                if (op.op == Add || op.op == Replace) transient.set(path, op.value.value());
                else if (op.op == Remove) transient.erase(path);
            }
            return transient.persistent();
        },
        [&](const open_file_dialog &a) { return FileDialog.set(a.dialog); },
        [&](const close_file_dialog &) { return set(FileDialog.Visible, false); },
        [&](const show_open_project_dialog &) { return FileDialog.set({"Choose file", AllProjectExtensionsDelimited, ".", ""}); },
        [&](const show_save_project_dialog &) { return FileDialog.set({"Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}); },
        [&](const show_open_faust_file_dialog &) { return FileDialog.set({"Choose file", FaustDspFileExtension, ".", ""}); },
        [&](const show_save_faust_file_dialog &) { return FileDialog.set({"Choose file", FaustDspFileExtension, ".", "my_dsp", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}); },
        [&](const show_save_faust_svg_file_dialog &) { return FileDialog.set({"Choose directory", ".*", ".", "faust_diagram", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}); },

        [&](const set_imgui_color_style &a) {
            switch (a.id) {
                case 0: return Style.ImGui.ColorsDark();
                case 1: return Style.ImGui.ColorsLight();
                case 2: return Style.ImGui.ColorsClassic();
                default: return store;
            }
        },
        [&](const set_implot_color_style &a) {
            switch (a.id) {
                case 0: return Style.ImPlot.ColorsAuto();
                case 1: return Style.ImPlot.ColorsDark();
                case 2: return Style.ImPlot.ColorsLight();
                case 3: return Style.ImPlot.ColorsClassic();
                default: return store;
            }
        },
        [&](const set_flowgrid_color_style &a) {
            switch (a.id) {
                case 0: return Style.FlowGrid.ColorsDark();
                case 1: return Style.FlowGrid.ColorsLight();
                case 2: return Style.FlowGrid.ColorsClassic();
                default: return store;
            }
        },
        [&](const set_flowgrid_diagram_color_style &a) {
            switch (a.id) {
                case 0: return Style.FlowGrid.DiagramColorsDark();
                case 1: return Style.FlowGrid.DiagramColorsLight();
                case 2: return Style.FlowGrid.DiagramColorsClassic();
                case 3: return Style.FlowGrid.DiagramColorsFaust();
                default: return store;
            }
        },
        [&](const set_flowgrid_diagram_layout_style &a) {
            switch (a.id) {
                case 0: return Style.FlowGrid.DiagramLayoutFlowGrid();
                case 1: return Style.FlowGrid.DiagramLayoutFaust();
                default: return store;
            }
        },
        [&](const open_faust_file &a) { return set(Audio.Faust.Code, FileIO::read(a.path)); },
        [&](const close_application &) { return set({{Processes.UI.Running, false}, {Audio.Running, false}}); },
        [&](const auto &) { return store; }, // All actions that don't directly update state (undo/redo & open/load-project, etc.)
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

void save_box_svg(const string &path); // Defined in FaustUI

void Context::on_action(const Action &action) {
    if (!action_allowed(action)) return; // Safeguard against actions running in an invalid state.

    const auto &base_path = std::holds_alternative<Actions::apply_patch>(action) ? std::get<Actions::apply_patch>(action).patch.base_path : RootPath;
    std::visit(visitor{
        // Handle actions that don't directly update state.
        // These options don't get added to the action/gesture history, since they only have non-application side effects,
        // and we don't want them replayed when loading a saved `.fga` project.
        [&](const Actions::open_project &a) { open_project(a.path); },
        [&](const open_empty_project &) { open_project(EmptyProjectPath); },
        [&](const open_default_project &) { open_project(DefaultProjectPath); },

        [&](const Actions::save_project &a) { save_project(a.path); },
        [&](const save_default_project &) { save_project(DefaultProjectPath); },
        [&](const Actions::save_current_project &) { save_project(current_project_path.value()); },
        [&](const save_faust_file &a) { FileIO::write(a.path, s.Audio.Faust.Code); },
        [&](const save_faust_svg_file &a) { save_box_svg(a.path); },

        // `store_history.index`-changing actions:
        [&](const undo &) {
            // `StoreHistory::SetIndex` reverts the current gesture before applying the new history index.
            // We want undo to only revert the gesture, without navigating back to before the previous gesture.
            store_history.SetIndex(history.active_gesture.empty() ? history.index - 1 : history.index);
        },
        [&](const redo &) { store_history.SetIndex(history.index + 1); },
        [&](const Actions::set_history_index &a) { store_history.SetIndex(a.index); },

        // Remaining actions have a direct effect on the application state.
        [&](const auto &a) {
            store_history.active_gesture.emplace_back(a);
            const auto prev_store = store;
            on_patch(CreatePatch(prev_store, set(state.Update(a)), base_path));
        },
    }, action);

    // Treat all toggles as immediate actions. Otherwise, performing two toggles in a row compresses into nothing.
    if (std::holds_alternative<toggle_value>(action)) store_history.FinalizeGesture();
}

//-----------------------------------------------------------------------------
// [SECTION] Context
//-----------------------------------------------------------------------------

#include <range/v3/view/filter.hpp>
#include <range/v3/view/map.hpp>
#include <utility>

Context::Context() {
    store_history.Reset();
    if (fs::exists(PreferencesPath)) {
        preferences = json::parse(FileIO::read(PreferencesPath));
    } else {
        write_preferences();
    }
}

Context::~Context() = default;

bool Context::is_user_project_path(const fs::path &path) {
    // Using relative path to avoid error: `filesystem error: in equivalent: Operation not supported`
    return !fs::equivalent(fs::relative(path), EmptyProjectPath) && !fs::equivalent(fs::relative(path), DefaultProjectPath);
}

void Context::save_empty_project() {
    save_project(EmptyProjectPath);
    if (!fs::exists(DefaultProjectPath)) save_project(DefaultProjectPath);
}

bool Context::clear_preferences() {
    preferences.recently_opened_paths.clear();
    return write_preferences();
}

json Context::get_project_json(const ProjectFormat format) {
    switch (format) {
        case None: return nullptr;
        case StateFormat: return store;
        case DiffFormat: return {{"diffs", history.Patches()}, {"index", history.index}};
        case ActionFormat: return history.Gestures();
    }
}

void Context::enqueue_action(const Action &a) { queued_actions.push(a); }

void Context::run_queued_actions(bool force_finalize_gesture) {
    if (!queued_actions.empty()) gesture_start_time = Clock::now();
    while (!queued_actions.empty()) {
        on_action(queued_actions.front());
        queued_actions.pop();
    }
    gesture_time_remaining_sec = max(0.0f, s.ApplicationSettings.GestureDurationSec - fsec(Clock::now() - gesture_start_time).count());
    if (!(is_widget_gesturing || gesture_time_remaining_sec > 0) || force_finalize_gesture) store_history.FinalizeGesture();
}

bool Context::action_allowed(const ActionID action_id) const {
    switch (action_id) {
        case action::id<undo>: return history.CanUndo();
        case action::id<redo>: return history.CanRedo();
        case action::id<Actions::open_default_project>: return fs::exists(DefaultProjectPath);
        case action::id<Actions::save_project>:
        case action::id<Actions::show_save_project_dialog>:
        case action::id<Actions::save_default_project>: return !history.Empty();
        case action::id<Actions::save_current_project>: return current_project_path.has_value() && !history.Empty();
        case action::id<Actions::open_file_dialog>: return !s.FileDialog.Visible;
        case action::id<Actions::close_file_dialog>: return s.FileDialog.Visible;
        default: return true;
    }
}
bool Context::action_allowed(const Action &action) const { return action_allowed(action::get_id(action)); }

void Context::clear() {
    current_project_path.reset();
    store_history.Reset();
    is_widget_gesturing = false;
}

// Private methods

ProjectFormat get_project_format(const fs::path &path) {
    const string &ext = path.extension();
    return ProjectFormatForExtension.contains(ext) ? ProjectFormatForExtension.at(ext) : None;
}

void Context::open_project(const fs::path &path) {
    const auto format = get_project_format(path);
    if (format == None) return; // TODO log

    clear();

    const json project = json::parse(FileIO::read(path));
    if (format == StateFormat) {
        set(store_from_json(project));

        s.Apply(UIContext::Flags_ImGuiSettings | UIContext::Flags_ImGuiStyle | UIContext::Flags_ImPlotStyle);
    } else if (format == DiffFormat) {
        open_project(EmptyProjectPath); // todo wasteful - need a `set_project_file` method or somesuch to avoid redoing other `open_project` side-effects.

//        diffs = project["diffs"]; // todo
        int new_index = project["history_index"];
        on_action(Actions::set_history_index{new_index});
    } else if (format == ActionFormat) {
        open_project(EmptyProjectPath);

        const Gestures project_gestures = project;
        for (const auto &gesture: project_gestures) {
            for (const auto &action: gesture) on_action(action);
            store_history.FinalizeGesture();
        }
    }

    if (is_user_project_path(path)) set_current_project_path(path);
}

bool Context::save_project(const fs::path &path) {
    if (current_project_path.has_value() && fs::equivalent(path, current_project_path.value()) &&
        !action_allowed(action::id<save_current_project>))
        return false;

    const auto format = get_project_format(path);
    if (format == None) return false; // TODO log

    store_history.FinalizeGesture(); // Make sure any pending actions/diffs are committed.
    if (FileIO::write(path, get_project_json(format).dump())) {
        if (is_user_project_path(path)) set_current_project_path(path);
        return true;
    }
    return false;
}

void Context::set_current_project_path(const fs::path &path) {
    current_project_path = path;
    preferences.recently_opened_paths.remove(path);
    preferences.recently_opened_paths.emplace_front(path);
    write_preferences();
}

bool Context::write_preferences() const {
    return FileIO::write(PreferencesPath, json(preferences).dump());
}

//-----------------------------------------------------------------------------
// [SECTION] History
//-----------------------------------------------------------------------------

StatePatch StoreHistory::CreatePatch(const int history_index) const {
    const size_t i = history_index == -1 ? index : history_index;
    return {::CreatePatch(store_records[i].store, store_records[i + 1].store), store_records[i + 1].time};
}

void StoreHistory::Reset() {
    store_records.clear();
    store_records.push_back({Clock::now(), store, {}});
    index = 0;
    active_gesture = {};
    stats = {};
}

int StoreHistory::Size() const { return int(store_records.size()); }
bool StoreHistory::Empty() const { return Size() == 0; }
bool StoreHistory::CanUndo() const { return !active_gesture.empty() || index > 0; }
bool StoreHistory::CanRedo() const { return index < Size(); }

vector<StatePatch> StoreHistory::Patches() const {
    return views::ints(0, Size() - 1) | transform([this](const int i) { return CreatePatch(i); }) | to<vector>;
}
vector<Gesture> StoreHistory::Gestures() const {
    return store_records | transform([](const auto &record) { return record.gesture; }) | to<vector>;
}

void StoreHistory::FinalizeGesture() {
    if (active_gesture.empty()) return;

    const auto gesture_patch = ::CreatePatch(store_records[index].store, store);
    stats.Apply(gesture_patch, Clock::now(), Forward, true);

    const auto merged_gesture = action::merge_gesture(active_gesture);
    active_gesture.clear();

    if (merged_gesture.empty()) {
        if (!gesture_patch.empty()) throw std::runtime_error("Non-empty patch resulting from an empty merged gesture.");
        return;
    }

    while (Size() > index + 1) store_records.pop_back(); // TODO use an undo _tree_ and keep this history
    store_records.push_back({Clock::now(), store, merged_gesture});
    index = Size() - 1;
}

void StoreHistory::Stats::Apply(const Patch &patch, TimePoint time, Direction direction, bool is_full_gesture) {
    if (!patch.empty()) latest_updated_paths = patch.ops | transform([&patch](const auto &entry) { return patch.base_path / entry.first; }) | to<vector>;

    for (const auto &[partial_path, op]: patch.ops) {
        const auto &path = patch.base_path / partial_path;
        if (direction == Forward) {
            auto &update_times = is_full_gesture ? committed_update_times_for_path : gesture_update_times_for_path;
            update_times[path].emplace_back(is_full_gesture && gesture_update_times_for_path.contains(path) ? gesture_update_times_for_path.at(path).back() : time);
        } else if (committed_update_times_for_path.contains(path)) {
            auto &update_times = committed_update_times_for_path.at(path);
            update_times.pop_back();
            if (update_times.empty()) committed_update_times_for_path.erase(path);
        }
    }

    if (is_full_gesture) gesture_update_times_for_path.clear();
}

std::optional<TimePoint> StoreHistory::Stats::LatestUpdateTime(const StatePath &path) const {
    if (gesture_update_times_for_path.contains(path)) return gesture_update_times_for_path.at(path).back();
    if (committed_update_times_for_path.contains(path)) return committed_update_times_for_path.at(path).back();
    return {};
}

StoreHistory::Stats::Plottable StoreHistory::Stats::CreatePlottable() const {
    vector<StatePath> paths;
    for (const auto &path: views::keys(committed_update_times_for_path)) paths.emplace_back(path);
    for (const auto &path: views::keys(gesture_update_times_for_path)) if (!committed_update_times_for_path.contains(path)) paths.emplace_back(path);

    const bool has_gesture = !gesture_update_times_for_path.empty();
    vector<ImU64> values(has_gesture ? paths.size() * 2 : paths.size());
    int i = 0;
    for (const auto &path: paths) values[i++] = committed_update_times_for_path.contains(path) ? committed_update_times_for_path.at(path).size() : 0;
    // Optionally add a second plot item for gesturing update times. See `ImPlot::PlotBarGroups` for value ordering explanation.
    if (has_gesture) for (const auto &path: paths) values[i++] = gesture_update_times_for_path.contains(path) ? gesture_update_times_for_path.at(path).size() : 0;

    const auto labels = paths | transform([](const string &path) {
        // Convert `string` to char array, removing first character of the path, which is a '/'.
        char *label = new char[path.size()];
        std::strcpy(label, string{path.begin() + 1, path.end()}.c_str());
        return label;
    }) | to<vector<const char *>>;

    return {labels, values};
}

void StoreHistory::SetIndex(int new_index) {
    // If we're mid-gesture, cancel the current gesture and navigate to the requested history index.
    if (!active_gesture.empty()) {
        // Cancel the gesture.
        stats.Apply({}, Clock::now(), Forward, true);
        active_gesture.clear();
        // Revert the gesture.
        const auto prev_store = store;
        const auto &patch = ::CreatePatch(prev_store, set(store_records[index].store));
        for (const auto &[partial_path, _op]: patch.ops) on_set_value(patch.base_path / partial_path);
        s.Audio.update_process();
    }
    if (new_index == index || new_index < 0 || new_index >= Size()) return;

    int old_index = index;
    index = new_index;

    const auto prev_store = store;
    const auto &patch = ::CreatePatch(prev_store, set(store_records[index].store));
    for (const auto &[partial_path, _op]: patch.ops) on_set_value(patch.base_path / partial_path);
    s.Audio.update_process();

    const auto direction = new_index > old_index ? Forward : Reverse;
    int i = old_index;
    while (i != new_index) {
        const auto &segment_patch = CreatePatch(direction == Reverse ? --i : i++);
        stats.Apply(segment_patch.Patch, segment_patch.Time, direction, true);
    }
}
