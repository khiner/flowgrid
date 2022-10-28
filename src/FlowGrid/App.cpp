#include "App.h"
#include "StateJson.h"

#include "immer/map.hpp"
#include "immer/map_transient.hpp"
#include <immer/algorithm.hpp>
#include "ImGuiFileDialog.h"

std::map<ImGuiID, StateMember *> StateMember::WithID{};

using StateMap = immer::map<string, Primitive>;
using StateMapTransient = immer::map_transient<string, Primitive>;

StateMap state_map; // All application state is stored in this canonical map.
StateMap gesture_begin_state; // Only updated on gesture-end (for diff calculation).
vector<std::pair<TimePoint, StateMap>> state_history; // One state checkpoint for every gesture.

inline Primitive get(const JsonPath &path) {
    return state_map.at(path.to_string());
}
inline void set(const JsonPath &path, Primitive value) {
    state_map = state_map.set(path.to_string(), std::move(value));
}

namespace nlohmann {
inline void to_json(json &j, const StateMap &v) {
    for (const auto &[key, value]: v) j[JsonPath(key)] = value;
}
}
// `from_json` defined out of `nlohmann`, to be called manually.
// This avoids getting a reference arg to a default-constructed, non-transient `StateMap` instance.
StateMap state_from_json(const json &j) {
    StateMapTransient _state;
    const auto &flattened = j.flatten();
    vector<std::pair<JsonPath, Primitive>> items(flattened.size());
    int item_index = 0;
    for (const auto &it: flattened.items()) items[item_index++] = {JsonPath(it.key()), Primitive(it.value())};

    for (size_t i = 0; i < items.size(); i++) {
        const auto &[path, value] = items[i];
        if (path.back() == "w" && i < items.size() - 3 && items[i + 3].first.back() == "z") {
            const float w = std::get<float>(value);
            const float x = std::get<float>(items[i + 1].second);
            const float y = std::get<float>(items[i + 2].second);
            const float z = std::get<float>(items[i + 3].second);
            _state.set(path.parent_pointer().to_string(), ImVec4{x, y, z, w});
            i += 3;
        } else if (path.back() == "x" && i < items.size() - 1 && items[i + 1].first.back() == "y") {
            const float x = std::get<float>(value);
            const float y = std::get<float>(items[i + 1].second);
            _state.set(path.parent_pointer().to_string(), ImVec2{x, y});
            i += 1;
        } else {
            _state.set(path.to_string(), value);
        }
    }
    return _state.persistent();
}

namespace Field {
Bool::operator bool() const { return std::get<bool>(get(Path)); }
Bool &Bool::operator=(bool value) {
    set(Path, value);
    return *this;
}

Int::operator int() const { return std::get<int>(get(Path)); }
Int &Int::operator=(int value) {
    set(Path, value);
    return *this;
}

Float::operator float() const { return std::get<float>(get(Path)); }
Float &Float::operator=(float value) {
    set(Path, value);
    return *this;
}

Vec2::operator ImVec2() const { return std::get<ImVec2>(get(Path)); }
Vec2 &Vec2::operator=(ImVec2 value) {
    set(Path, value);
    return *this;
}

String::operator string() const { return std::get<string>(get(Path)); }
bool String::operator==(const string &v) const { return string(*this) == v; }
String::operator bool() const { return !string(*this).empty(); }
String &String::operator=(string value) {
    set(Path, std::move(value));
    return *this;
}

Enum::operator int() const { return std::get<int>(get(Path)); }
Enum &Enum::operator=(int value) {
    set(Path, value);
    return *this;
}

Flags::operator int() const { return std::get<int>(get(Path)); }
Flags &Flags::operator=(int value) {
    set(Path, value);
    return *this;
}

Color::operator ImVec4() const { return std::get<ImVec4>(get(Path)); }
Color &Color::operator=(ImVec4 value) {
    set(Path, value);
    return *this;
}
}

string to_string(const IO io, const bool shorten) {
    switch (io) {
        case IO_In: return shorten ? "in" : "input";
        case IO_Out: return shorten ? "out" : "output";
        case IO_None: return "none";
    }
}

namespace action {
// An action's menu label is its name, except for a few exceptions.
const std::map<ID, string> menu_label_for_id{
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

// Inspired by [`lager`](https://sinusoid.es/lager/architecture.html#reducer), but only the action-visitor pattern remains.
void State::Update(const Action &action) {
    std::visit(visitor{
        [&](const show_open_project_dialog &) { FileDialog = {"Choose file", AllProjectExtensionsDelimited, ".", ""}; },
        [&](const show_save_project_dialog &) { FileDialog = {"Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}; },
        [&](const show_open_faust_file_dialog &) { FileDialog = {"Choose file", FaustDspFileExtension, ".", ""}; },
        [&](const show_save_faust_file_dialog &) { FileDialog = {"Choose file", FaustDspFileExtension, ".", "my_dsp", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}; },
        [&](const show_save_faust_svg_file_dialog &) { FileDialog = {"Choose directory", ".*", ".", "faust_diagram", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}; },

        [&](const open_file_dialog &a) { FileDialog = a.dialog; },
        [&](const close_file_dialog &) { FileDialog.Visible = false; },

        [&](const set_imgui_settings &a) { ImGuiSettings = a.settings; },
        [&](const set_imgui_color_style &a) {
            switch (a.id) {
                case 0: Style.ImGui.ColorsDark();
                    break;
                case 1: Style.ImGui.ColorsLight();
                    break;
                case 2: Style.ImGui.ColorsClassic();
                    break;
            }
        },
        [&](const set_implot_color_style &a) {
            switch (a.id) {
                case 0: Style.ImPlot.ColorsAuto();
                    break;
                case 1: Style.ImPlot.ColorsDark();
                    break;
                case 2: Style.ImPlot.ColorsLight();
                    break;
                case 3: Style.ImPlot.ColorsClassic();
                    break;
            }
        },
        [&](const set_flowgrid_color_style &a) {
            switch (a.id) {
                case 0: Style.FlowGrid.ColorsDark();
                    break;
                case 1: Style.FlowGrid.ColorsLight();
                    break;
                case 2: Style.FlowGrid.ColorsClassic();
                    break;
            }
        },
        [&](const set_flowgrid_diagram_color_style &a) {
            switch (a.id) {
                case 0: Style.FlowGrid.DiagramColorsDark();
                    break;
                case 1: Style.FlowGrid.DiagramColorsLight();
                    break;
                case 2: Style.FlowGrid.DiagramColorsClassic();
                    break;
                case 3: Style.FlowGrid.DiagramColorsFaust();
                    break;
            }
        },
        [&](const set_flowgrid_diagram_layout_style &a) {
            switch (a.id) {
                case 0: Style.FlowGrid.DiagramLayoutFlowGrid();
                    break;
                case 1: Style.FlowGrid.DiagramLayoutFaust();
                    break;
            }
        },

        [&](const open_faust_file &a) { Audio.Faust.Code = FileIO::read(a.path); },

        [&](const close_application &) {
            Processes.UI.Running = false;
            Audio.Running = false;
        },

        [&](const auto &) {}, // All actions that don't directly update state (e.g. undo/redo & open/load-project)
    }, action);
}

void save_box_svg(const string &path); // defined in FaustUI

// todo Move away from JsonPatches entirely. This is only an intermediate step.
//   In the future, we won't even need to precompute diffs! Just calculate them on the fly from the snapshots as needed (when viewing etc)
JsonPatch create_patch(const StateMap &before_state, const StateMap &after_state) {
    JsonPatch patch;
    diff(
        before_state,
        after_state,
        [&](auto const &added_element) {
            patch.push_back({JsonPath(added_element.first), Add, added_element.second});
        },
        [&](auto const &removed_element) {
            patch.push_back({JsonPath(removed_element.first), Remove, removed_element.second});
        },
        [&](auto const &old_element, auto const &new_element) {
            patch.push_back({JsonPath(old_element.first), Replace, new_element.second});
        });

    return patch;
}

void Context::on_action(const Action &action) {
    if (!action_allowed(action)) return; // Safeguard against actions running in an invalid state.

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

        // `state_history_index`-changing actions:
        [&](const undo &) { increment_history_index(-1); },
        [&](const redo &) { increment_history_index(1); },
        [&](const Actions::set_history_index &a) {
            if (!active_gesture_patch.empty()) finalize_gesture(); // Make sure any pending actions/diffs are committed.
            set_history_index(a.history_index);
        },

        // Remaining actions have a direct effect on the application state.
        // Keep JSON & struct versions of state in sync.
        [&](const set_value &a) {
            const auto before_state = state_map;
            set(a.path, a.value);
            on_patch(a, create_patch(before_state, state_map));
        },
        [&](const set_values &a) {
            const auto before_state = state_map;
            // See https://sinusoid.es/immer/design.html#leveraging-move-semantics
            for (const auto &[path, value]: a.values) state_map = std::move(state_map).set(path.to_string(), value);
            on_patch(a, create_patch(before_state, state_map));
        },
        [&](const toggle_value &a) {
            const auto before_state = state_map;
            set(a.path, !std::get<bool>(get(a.path)));
            on_patch(a, create_patch(before_state, state_map));
            // Treat all toggles as immediate actions. Otherwise, performing two toggles in a row and undoing does nothing, since they're compressed into nothing.
            finalize_gesture();
        },
        [&](const auto &a) {
            const auto before_state = state_map;
            state.Update(a);
            on_patch(a, create_patch(before_state, state_map));
        },
    }, action);
}

//-----------------------------------------------------------------------------
// [SECTION] Context
//-----------------------------------------------------------------------------

#include <range/v3/view/filter.hpp>
#include <range/v3/view/map.hpp>
#include <utility>

Context::Context() {
    state_history.emplace_back(Clock::now(), state_map);
    gesture_begin_state = state_map;
    if (fs::exists(PreferencesPath)) {
        preferences = json::parse(FileIO::read(PreferencesPath));
    } else {
        write_preferences();
    }
}

Context::~Context() = default;

int Context::history_size() { return int(state_history.size()); }

StatePatch Context::create_diff(int history_index) {
    return {
        create_patch(state_history[history_index].second, state_history[history_index + 1].second),
        state_history[history_index + 1].first,
    };
}

bool Context::is_user_project_path(const fs::path &path) {
    // Using relative path to avoid error: `filesystem error: in equivalent: Operation not supported`
    return !fs::equivalent(fs::relative(path), EmptyProjectPath) && !fs::equivalent(fs::relative(path), DefaultProjectPath);
}

bool Context::project_has_changes() const { return gestures.size() != project_start_gesture_count; }

void Context::save_empty_project() {
    save_project(EmptyProjectPath);
    if (!fs::exists(DefaultProjectPath)) save_project(DefaultProjectPath);
}

bool Context::clear_preferences() {
    preferences.recently_opened_paths.clear();
    return write_preferences();
}

json Context::get_project_json(const ProjectFormat format) const {
    switch (format) {
        case None: return nullptr;
        case StateFormat: return state_map;
        case DiffFormat:
            return {{"diffs", views::ints(0, int(state_history.size() - 1)) | transform(create_diff) | to<vector<StatePatch>>},
                    {"history_index", state_history_index}};
        case ActionFormat: return gestures;
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
    if (!(is_widget_gesturing || gesture_time_remaining_sec > 0) || force_finalize_gesture) finalize_gesture();
}

bool Context::action_allowed(const ActionID action_id) const {
    switch (action_id) {
        case action::id<undo>: return !active_gesture_patch.empty() || state_history_index > 0;
        case action::id<redo>: return state_history_index < int(state_history.size());
        case action::id<Actions::open_default_project>: return fs::exists(DefaultProjectPath);
        case action::id<Actions::save_project>:
        case action::id<Actions::show_save_project_dialog>:
        case action::id<Actions::save_default_project>: return project_has_changes();
        case action::id<Actions::save_current_project>: return current_project_path.has_value() && project_has_changes();
        case action::id<Actions::open_file_dialog>: return !s.FileDialog.Visible;
        case action::id<Actions::close_file_dialog>: return s.FileDialog.Visible;
        default: return true;
    }
}
bool Context::action_allowed(const Action &action) const { return action_allowed(action::get_id(action)); }

void Context::update_ui_context(UIContextFlags flags) {
    if (flags == UIContextFlags_None) return;

    if (flags & UIContextFlags_ImGuiSettings) s.ImGuiSettings.Apply(ui->imgui_context);
    if (flags & UIContextFlags_ImGuiStyle) s.Style.ImGui.Apply(ui->imgui_context);
    if (flags & UIContextFlags_ImPlotStyle) s.Style.ImPlot.Apply(ui->implot_context);
}

void Context::update_faust_context() {
    if (s.Audio.OutSampleRate == 0) return; // Sample rate has not been set up yet (set during first audio stream initialization).

    has_new_faust_code = true; // todo I hate this. also, might be called due to sample rate change, not code change.
}

void Context::clear() {
    current_project_path.reset();
    state_history.clear();
    state_history.emplace_back(Clock::now(), state_map);
    state_history_index = 0;
    gesture_begin_state = state_map;
    gestures.clear();
    project_start_gesture_count = gestures.size();
    is_widget_gesturing = false;
    state_stats = {};
    // todo finalize?
    active_gesture = {};
    active_gesture_patch = {};
}

// StateStats

void StateStats::apply_patch(const JsonPatch &patch, TimePoint time, Direction direction, bool is_full_gesture) {
    if (!patch.empty()) latest_updated_paths = {};

    for (const JsonPatchOp &patch_op: patch) {
        // For add/remove ops, the thing being updated is the _parent_.
        const JsonPath &path = patch_op.op == Add || patch_op.op == Remove ? patch_op.path.parent_pointer() : patch_op.path;
        latest_updated_paths.emplace_back(path);

        if (direction == Forward) {
            auto &update_times_for_path = is_full_gesture ? committed_update_times_for_path : gesture_update_times_for_path;
            update_times_for_path[path].emplace_back(is_full_gesture && gesture_update_times_for_path.contains(path) ? gesture_update_times_for_path.at(path).back() : time);
        } else {
            // Undo never applies to `gesture_update_times_for_path`
            auto &update_times = committed_update_times_for_path.at(path);
            update_times.pop_back();
            if (update_times.empty()) committed_update_times_for_path.erase(path);
        }

        const bool path_in_gesture = gesture_update_times_for_path.contains(path);
        const bool path_in_committed = committed_update_times_for_path.contains(path);
        if (path_in_gesture || path_in_committed) {
            latest_update_time_for_path[path] = path_in_gesture ? gesture_update_times_for_path.at(path).back() : committed_update_times_for_path.at(path).back();
        } else {
            latest_update_time_for_path.erase(path);
        }
    }

    if (is_full_gesture) gesture_update_times_for_path.clear();
    PathUpdateFrequency = create_path_update_frequency_plottable();
}

StateStats::Plottable StateStats::create_path_update_frequency_plottable() {
    vector<JsonPath> paths;
    for (const auto &path: views::keys(committed_update_times_for_path)) paths.emplace_back(path);
    for (const auto &path: views::keys(gesture_update_times_for_path)) {
        if (!committed_update_times_for_path.contains(path)) paths.emplace_back(path);
    }

    const bool has_gesture = !gesture_update_times_for_path.empty();
    vector<ImU64> values(has_gesture ? paths.size() * 2 : paths.size());
    int i = 0;
    for (const auto &path: paths) {
        values[i++] = committed_update_times_for_path.contains(path) ? committed_update_times_for_path.at(path).size() : 0;
    }
    // Optionally add a second plot item for gesturing update times. See `ImPlot::PlotBarGroups` for value ordering explanation.
    if (has_gesture) {
        for (const auto &path: paths) {
            values[i++] = gesture_update_times_for_path.contains(path) ? gesture_update_times_for_path.at(path).size() : 0;
        }
    }

    const auto labels = paths | transform([](const string &path) {
        // Convert `string` to char array, removing first character of the path, which is a '/'.
        char *label = new char[path.size()];
        std::strcpy(label, string{path.begin() + 1, path.end()}.c_str());
        return label;
    }) | to<vector<const char *>>;

    return {labels, values};
}

// Private methods

void Context::finalize_gesture() {
    if (active_gesture.empty()) return;

    state_stats.apply_patch(active_gesture_patch, Clock::now(), Forward, true);

    const auto merged_gesture = action::merge_gesture(active_gesture);
    active_gesture.clear();

    const auto merged_gesture_size = merged_gesture.size();
    // Apply context-dependent transformations to actions with large data members to compress them before committing them to the gesture history.
    const auto active_gesture_compressed = merged_gesture | transform([this, merged_gesture_size](const auto &action) -> Action {
        const auto id = action::get_id(action);
        if (id == action::id<Actions::set_history_index> && merged_gesture_size == 1) {
            const auto new_history_index = std::get<Actions::set_history_index>(action).history_index;
            if (new_history_index == gesture_begin_history_index - 1) return undo{};
            else if (new_history_index == gesture_begin_history_index + 1) return redo{};
        }
        return action;
    }) | views::filter([this](const auto &action) {
        // Filter out any resulting actions that don't actually result in a `state_history_index` change.
        return action::get_id(action) != action::id<Actions::set_history_index> || std::get<Actions::set_history_index>(action).history_index != gesture_begin_history_index;
    }) | to<const Gesture>;
    if (!active_gesture_compressed.empty()) gestures.emplace_back(active_gesture_compressed);

    gesture_begin_history_index = state_history_index;
    if (active_gesture_patch.empty()) return;
    if (active_gesture_compressed.empty()) throw std::runtime_error("Non-empty state-diff resulting from an empty compressed gesture!");

    // TODO use an undo _tree_ and keep this history
    while (int(state_history.size()) > state_history_index + 1) state_history.pop_back();
    state_history.emplace_back(Clock::now(), state_map);
    state_history_index = int(state_history.size()) - 1;
    gesture_begin_history_index = state_history_index;
    gesture_begin_state = state_map;
    active_gesture_patch.clear();
}

void Context::on_patch(const Action &action, const JsonPatch &patch) {
    active_gesture.emplace_back(action);
    active_gesture_patch = create_patch(gesture_begin_state, state_map);

    state_stats.apply_patch(patch, Clock::now(), Forward, false);
    for (const auto &patch_op: patch) on_set_value(patch_op.path);
    s.Audio.update_process();
}

void Context::set_history_index(int new_history_index) {
    if (new_history_index == state_history_index || new_history_index < 0 || new_history_index >= int(state_history.size())) return;

    active_gesture.emplace_back(Actions::set_history_index{new_history_index});

    const auto direction = new_history_index > state_history_index ? Forward : Reverse;
    // todo set index directly instead of incrementing - just need to update `state_stats` to reflect recent immer changes
    while (state_history_index != new_history_index) {
        const auto index = direction == Reverse ? --state_history_index : ++state_history_index;
        const auto before_state = state_map;
        state_map = state_history[index].second;
        const auto &patch = direction == Reverse ? create_patch(state_map, before_state) : create_patch(before_state, state_map);
        gesture_begin_state = state_map;
        state_stats.apply_patch(patch, state_history[index].first, direction, true);
        for (const auto &patch_op: patch) on_set_value(patch_op.path);
    }
    s.Audio.update_process();
}

void Context::increment_history_index(int delta) {
    if (!active_gesture_patch.empty()) finalize_gesture(); // Make sure any pending actions/diffs are committed. _This can change `state_history_index`!_
    set_history_index(state_history_index + delta);
}

void Context::on_set_value(const JsonPath &path) {
    const auto &path_str = path.to_string();

    // Setting `ImGuiSettings` does not require a `c.update_ui_context` on the action, since the action will be initiated by ImGui itself,
    // whereas the style editors don't update the ImGui/ImPlot contexts themselves.
    if (path_str.rfind(s.ImGuiSettings.Path.to_string(), 0) == 0) update_ui_context(UIContextFlags_ImGuiSettings); // TODO only when not ui-initiated
    else if (path_str.rfind(s.Style.ImGui.Path.to_string(), 0) == 0) update_ui_context(UIContextFlags_ImGuiStyle); // TODO add `starts_with` method to nlohmann/json?
    else if (path_str.rfind(s.Style.ImPlot.Path.to_string(), 0) == 0) update_ui_context(UIContextFlags_ImPlotStyle);
    else if (path == s.Audio.Faust.Code.Path || path == s.Audio.OutSampleRate.Path) update_faust_context();
}

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
        state_map = state_from_json(project);
        gesture_begin_state = state_map;

        update_ui_context(UIContextFlags_ImGuiSettings | UIContextFlags_ImGuiStyle | UIContextFlags_ImPlotStyle);
        update_faust_context();
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
            finalize_gesture();
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

    finalize_gesture(); // Make sure any pending actions/diffs are committed.
    if (FileIO::write(path, get_project_json(format).dump())) {
        if (is_user_project_path(path)) set_current_project_path(path);
        return true;
    }
    return false;
}

void Context::set_current_project_path(const fs::path &path) {
    current_project_path = path;
    project_start_gesture_count = gestures.size();
    preferences.recently_opened_paths.remove(path);
    preferences.recently_opened_paths.emplace_front(path);
    write_preferences();
}

bool Context::write_preferences() const {
    return FileIO::write(PreferencesPath, json(preferences).dump());
}
