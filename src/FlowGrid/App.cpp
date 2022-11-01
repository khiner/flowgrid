#include "App.h"
#include "StateJson.h"

#include <immer/algorithm.hpp>
#include "ImGuiFileDialog.h"

std::map<ImGuiID, StateMember *> StateMember::WithID{};

Store gesture_begin_store; // Only updated on gesture-end (for diff calculation).
vector<std::pair<TimePoint, Store>> store_history; // One store checkpoint for every gesture.

Store Context::set(Store persistent) { return store = std::move(persistent); }
Store Context::set(TransientStore transient) { return store = transient.persistent(); }

Primitive get(const JsonPath &path) { return c.sm.at(path.to_string()); }
Store set(const JsonPath &path, const Primitive &value, const Store &store) { return store.set(path.to_string(), value); }
Store set(const StateMember &member, const Primitive &value, const Store &store) { return store.set(member.Path.to_string(), value); }
Store set(const JsonPath &path, const ImVec4 &value, const Store &store) { return store.set(path.to_string(), value); }
Store remove(const JsonPath &path, const Store &store) { return store.erase(path.to_string()); }
Store set(const StoreEntries &values, const Store &store) {
    auto transient = store.transient();
    set(values, transient);
    return transient.persistent();
}
Store set(const MemberEntries &values, const Store &store) {
    auto transient = store.transient();
    set(values, transient);
    return transient.persistent();
}
Store set(const std::vector<std::pair<JsonPath, ImVec4>> &values, const Store &store) {
    auto transient = store.transient();
    set(values, transient);
    return transient.persistent();
}
// Transient equivalents
void set(const JsonPath &path, const Primitive &value, TransientStore &store) { return store.set(path.to_string(), value); }
void set(const StateMember &member, const Primitive &value, TransientStore &store) { store.set(member.Path.to_string(), value); }
void set(const JsonPath &path, const ImVec4 &value, TransientStore &store) { store.set(path.to_string(), value); }
void remove(const JsonPath &path, TransientStore &store) { store.erase(path.to_string()); }
void set(const StoreEntries &values, TransientStore &store) {
    for (const auto &[path, value]: values) store.set(path.to_string(), value);
}
void set(const MemberEntries &values, TransientStore &store) {
    for (const auto &[member, value]: values) store.set(member.Path.to_string(), value);
}
void set(const std::vector<std::pair<JsonPath, ImVec4>> &values, TransientStore &store) {
    for (const auto &[path, value]: values) store.set(path.to_string(), value);
}

StateMember::StateMember(const StateMember *parent, const string &id, const Primitive &value) : StateMember(parent, id) {
    c.set(set(Path, value));
}

namespace nlohmann {
inline void to_json(json &j, const Store &v) {
    for (const auto &[key, value]: v) j[JsonPath(key)] = value;
}
}

// `from_json` defined out of `nlohmann`, to be called manually.
// This avoids getting a reference arg to a default-constructed, non-transient `Store` instance.
Store store_from_json(const json &j) {
    const auto &flattened = j.flatten();
    StoreEntries entries(flattened.size());
    int item_index = 0;
    for (const auto &it: flattened.items()) entries[item_index++] = {JsonPath(it.key()), Primitive(it.value())};

    TransientStore _store;
    for (size_t i = 0; i < entries.size(); i++) {
        const auto &[path, value] = entries[i];
        if (path.back() == "w" && i < entries.size() - 3 && entries[i + 3].first.back() == "z") {
            const auto w = std::get<float>(value);
            const auto x = std::get<float>(entries[i + 1].second);
            const auto y = std::get<float>(entries[i + 2].second);
            const auto z = std::get<float>(entries[i + 3].second);
            _store.set(path.parent_pointer().to_string(), ImVec4{x, y, z, w});
            i += 3;
        } else if (path.back() == "x" && i < entries.size() - 1 && entries[i + 1].first.back() == "y") {
            if (std::holds_alternative<ImVec2ih>(value)) {
                const auto x = std::get<int>(value);
                const auto y = std::get<int>(entries[i + 1].second);
                _store.set(path.parent_pointer().to_string(), ImVec2ih{short(x), short(y)});
            } else {
                const auto x = std::get<float>(value);
                const auto y = std::get<float>(entries[i + 1].second);
                _store.set(path.parent_pointer().to_string(), ImVec2{x, y});
            }
            i += 1;
        } else {
            _store.set(path.to_string(), value);
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

Store State::Update(const Action &action) const {
    return std::visit(visitor{
        [&](const show_open_project_dialog &) { return FileDialog.set({"Choose file", AllProjectExtensionsDelimited, ".", ""}); },
        [&](const show_save_project_dialog &) { return FileDialog.set({"Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}); },
        [&](const show_open_faust_file_dialog &) { return FileDialog.set({"Choose file", FaustDspFileExtension, ".", ""}); },
        [&](const show_save_faust_file_dialog &) { return FileDialog.set({"Choose file", FaustDspFileExtension, ".", "my_dsp", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}); },
        [&](const show_save_faust_svg_file_dialog &) { return FileDialog.set({"Choose directory", ".*", ".", "faust_diagram", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}); },

        [&](const open_file_dialog &a) { return FileDialog.set(a.dialog); },
        [&](const close_file_dialog &) { return set(FileDialog.Visible, false); },

        [&](const set_imgui_color_style &a) {
            switch (a.id) {
                case 0: return Style.ImGui.ColorsDark();
                case 1: return Style.ImGui.ColorsLight();
                case 2: return Style.ImGui.ColorsClassic();
                default: return c.sm;
            }
        },
        [&](const set_implot_color_style &a) {
            switch (a.id) {
                case 0: return Style.ImPlot.ColorsAuto();
                case 1: return Style.ImPlot.ColorsDark();
                case 2: return Style.ImPlot.ColorsLight();
                case 3: return Style.ImPlot.ColorsClassic();
                default: return c.sm;
            }
        },
        [&](const set_flowgrid_color_style &a) {
            switch (a.id) {
                case 0: return Style.FlowGrid.ColorsDark();
                case 1: return Style.FlowGrid.ColorsLight();
                case 2: return Style.FlowGrid.ColorsClassic();
                default: return c.sm;
            }
        },
        [&](const set_flowgrid_diagram_color_style &a) {
            switch (a.id) {
                case 0: return Style.FlowGrid.DiagramColorsDark();
                case 1: return Style.FlowGrid.DiagramColorsLight();
                case 2: return Style.FlowGrid.DiagramColorsClassic();
                case 3: return Style.FlowGrid.DiagramColorsFaust();
                default: return c.sm;
            }
        },
        [&](const set_flowgrid_diagram_layout_style &a) {
            switch (a.id) {
                case 0: return Style.FlowGrid.DiagramLayoutFlowGrid();
                case 1: return Style.FlowGrid.DiagramLayoutFaust();
                default: return c.sm;
            }
        },
        [&](const open_faust_file &a) { return set(Audio.Faust.Code, FileIO::read(a.path)); },
        [&](const close_application &) {
            return set({
                {Processes.UI.Running, false},
                {Audio.Running, false},
            });
        },
        [&](const auto &) {
            return sm;
        }, // All actions that don't directly update state (e.g. undo/redo & open/load-project)
    }, action);
}

void save_box_svg(const string &path); // defined in FaustUI

// todo Move away from JsonPatches entirely. This is only an intermediate step.
//   In the future, we won't even need to precompute diffs! Just calculate them on the fly from the snapshots as needed (when viewing etc)
JsonPatch create_patch(const Store &before, const Store &after) {
    JsonPatch patch;
    diff(
        before,
        after,
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

        // `store_history_index`-changing actions:
        [&](const undo &) { increment_history_index(-1); },
        [&](const redo &) { increment_history_index(1); },
        [&](const Actions::set_history_index &a) {
            if (!active_gesture_patch.empty()) finalize_gesture(); // Make sure any pending actions/diffs are committed.
            set_history_index(a.history_index);
        },

        // Remaining actions have a direct effect on the application state.
        [&](const set_value &a) {
            const auto prev_store = store;
            set(::set(a.path, a.value));
            on_patch(a, create_patch(prev_store, store));
        },
        [&](const set_values &a) {
            const auto prev_store = store;
            set(::set(a.values));
            on_patch(a, create_patch(prev_store, store));
        },
        [&](const toggle_value &a) {
            const auto prev_store = store;
            set(::set(a.path, !std::get<bool>(get(a.path))));
            on_patch(a, create_patch(prev_store, store));
            // Treat all toggles as immediate actions. Otherwise, performing two toggles in a row and undoing does nothing, since they're compressed into nothing.
            finalize_gesture();
        },
        [&](const apply_patch &a) {
            // todo correct implementation
            StoreEntries values;
            vector<JsonPath> removed_paths;
            for (const auto &op: a.patch) {
                if (op.op == Add) values.emplace_back(op.path, op.value.value());
                else if (op.op == Remove) removed_paths.push_back(op.path);
                else if (op.op == Replace) values.emplace_back(op.path, op.value.value());
            }
            const auto prev_store = store;
            set(::set(values));
//            remove(::remove(removed_paths));
            on_patch(a, create_patch(prev_store, store));
        },
        [&](const auto &a) {
            on_patch(a, create_patch(store, set(state.Update(a))));
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
    store_history.emplace_back(Clock::now(), store);
    gesture_begin_store = store;
    if (fs::exists(PreferencesPath)) {
        preferences = json::parse(FileIO::read(PreferencesPath));
    } else {
        write_preferences();
    }
}

Context::~Context() = default;

int Context::history_size() { return int(store_history.size()); }

StatePatch Context::create_diff(int history_index) {
    return {
        create_patch(store_history[history_index].second, store_history[history_index + 1].second),
        store_history[history_index + 1].first,
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
        case StateFormat: return store;
        case DiffFormat:
            return {{"diffs", views::ints(0, int(store_history.size() - 1)) | transform(create_diff) | to<vector<StatePatch>>},
                    {"history_index", store_history_index}};
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
        case action::id<undo>: return !active_gesture_patch.empty() || store_history_index > 0;
        case action::id<redo>: return store_history_index < int(store_history.size());
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
    store_history.clear();
    store_history.emplace_back(Clock::now(), store);
    store_history_index = 0;
    gesture_begin_store = store;
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
            if (new_history_index == gesture_begin_history_index + 1) return redo{};
        }
        return action;
    }) | views::filter([this](const auto &action) {
        // Filter out any resulting actions that don't actually result in a `store_history_index` change.
        return action::get_id(action) != action::id<Actions::set_history_index> || std::get<Actions::set_history_index>(action).history_index != gesture_begin_history_index;
    }) | to<const Gesture>;
    if (!active_gesture_compressed.empty()) gestures.emplace_back(active_gesture_compressed);

    gesture_begin_history_index = store_history_index;
    if (active_gesture_patch.empty()) return;
    if (active_gesture_compressed.empty()) throw std::runtime_error("Non-empty state-diff resulting from an empty compressed gesture!");

    // TODO use an undo _tree_ and keep this history
    while (int(store_history.size()) > store_history_index + 1) store_history.pop_back();
    store_history.emplace_back(Clock::now(), store);
    store_history_index = int(store_history.size()) - 1;
    gesture_begin_history_index = store_history_index;
    gesture_begin_store = store;
    active_gesture_patch.clear();
}

void Context::on_patch(const Action &action, const JsonPatch &patch) {
    active_gesture.emplace_back(action);
    active_gesture_patch = create_patch(gesture_begin_store, store);

    state_stats.apply_patch(patch, Clock::now(), Forward, false);
    for (const auto &patch_op: patch) on_set_value(patch_op.path);
    s.Audio.update_process();
}

void Context::set_history_index(int new_history_index) {
    if (new_history_index == store_history_index || new_history_index < 0 || new_history_index >= int(store_history.size())) return;

    active_gesture.emplace_back(Actions::set_history_index{new_history_index});

    const auto direction = new_history_index > store_history_index ? Forward : Reverse;
    // todo set index directly instead of incrementing - just need to update `state_stats` to reflect recent immer changes
    while (store_history_index != new_history_index) {
        const auto index = direction == Reverse ? --store_history_index : ++store_history_index;
        const auto prev_store = store;
        store = store_history[index].second;
        const auto &patch = direction == Reverse ? create_patch(store, prev_store) : create_patch(prev_store, store);
        gesture_begin_store = store;
        state_stats.apply_patch(patch, store_history[index].first, direction, true);
        for (const auto &patch_op: patch) on_set_value(patch_op.path);
    }
    s.Audio.update_process();
}

void Context::increment_history_index(int delta) {
    if (!active_gesture_patch.empty()) finalize_gesture(); // Make sure any pending actions/diffs are committed. _This can change `store_history_index`!_
    set_history_index(store_history_index + delta);
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
        store = store_from_json(project);
        gesture_begin_store = store;

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
