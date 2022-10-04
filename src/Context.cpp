#include "Context.h"

#include <range/v3/view/filter.hpp>
#include <range/v3/view/map.hpp>

#include "UI/Faust/Diagram.h"

Context::Context() : state_json(state), gesture_begin_state_json(state) {
    if (fs::exists(PreferencesPath)) {
        preferences = json::from_msgpack(FileIO::read(PreferencesPath));
    } else {
        write_preferences();
    }
}

Context::~Context() = default;

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

json Context::get_project_json(const ProjectFormat format) {
    switch (format) {
        case None: return nullptr;
        case StateFormat: return sj;
        case DiffFormat:
            return {{"diffs", diffs},
                    {"diff_index", diff_index}};
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
        case action::id<undo>: return !active_gesture_patch.empty() || diff_index >= 0;
        case action::id<redo>: return diff_index < (int) diffs.size() - 1;
        case action::id<Actions::open_default_project>: return fs::exists(DefaultProjectPath);
        case action::id<Actions::save_project>:
        case action::id<Actions::show_save_project_dialog>:
        case action::id<Actions::save_default_project>: return project_has_changes();
        case action::id<Actions::save_current_project>: return current_project_path.has_value() && project_has_changes();
        case action::id<Actions::open_file_dialog>: return !s.File.Dialog.Visible;
        case action::id<Actions::close_file_dialog>: return s.File.Dialog.Visible;
        default: return true;
    }
}
bool Context::action_allowed(const Action &action) const { return action_allowed(action::get_id(action)); }

// TODO Implement
//  ```cpp
//  std::pair<Diff, Diff> {forward_diff, reverse_diff} = json::bidirectional_diff(old_state_json, new_state_json);
//  ```
//  https://github.com/nlohmann/json/discussions/3396#discussioncomment-2513010

void Context::update_ui_context(UIContextFlags flags) {
    if (flags == UIContextFlags_None) return;

    if (flags & UIContextFlags_ImGuiSettings) s.ImGuiSettings.apply(ui->imgui_context);
    if (flags & UIContextFlags_ImGuiStyle) s.Style.ImGui.apply(ui->imgui_context);
    if (flags & UIContextFlags_ImPlotStyle) s.Style.ImPlot.apply(ui->implot_context);
}

void Context::update_faust_context() {
    if (s.Audio.OutSampleRate == 0) return; // Sample rate has not been set up yet (set during first audio stream initialization).

    has_new_faust_code = true; // todo I hate this. also, might be called due to sample rate change, not code change.
}

void Context::clear() {
    diff_index = -1;
    current_project_path.reset();
    diffs.clear();
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
    std::vector<JsonPath> paths;
    for (const auto &path: views::keys(committed_update_times_for_path)) paths.emplace_back(path);
    for (const auto &path: views::keys(gesture_update_times_for_path)) {
        if (!committed_update_times_for_path.contains(path)) paths.emplace_back(path);
    }

    const bool has_gesture = !gesture_update_times_for_path.empty();
    std::vector<ImU64> values(has_gesture ? paths.size() * 2 : paths.size());
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
    }) | to<std::vector<const char *>>;

    return {labels, values};
}

// Private methods

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

        // `diff_index`-changing actions:
        [&](const undo &) { increment_diff_index(-1); },
        [&](const redo &) { increment_diff_index(1); },
        [&](const Actions::set_diff_index &a) {
            if (!active_gesture_patch.empty()) finalize_gesture(); // Make sure any pending actions/diffs are committed.
            set_diff_index(a.diff_index);
        },

        // Remaining actions have a direct effect on the application state.
        // Keep JSON & struct versions of state in sync.
        [&](const set_value &a) {
            const auto before_json = state_json;
            state_json[a.path] = a.value;
            state = state_json;
            on_patch(a, json::diff(before_json, state_json));
        },
        [&](const set_values &a) {
            const auto before_json = state_json;
            for (const auto &[path, value]: a.values) {
                state_json[path] = value;
            }
            state = state_json;
            on_patch(a, json::diff(before_json, state_json));
        },
        [&](const toggle_value &a) {
            const auto before_json = state_json;
            state_json[a.path] = !state_json[a.path];
            state = state_json;
            on_patch(a, json::diff(before_json, state_json));
            // Treat all toggles as immediate actions. Otherwise, performing two toggles in a row and undoing does nothing, since they're compressed into nothing.
            finalize_gesture();
        },
        [&](const auto &a) {
            const auto before_json = state_json;
            state.update(a);
            state_json = state;
            on_patch(a, json::diff(before_json, state_json));
        },
    }, action);
}

void Context::finalize_gesture() {
    if (active_gesture.empty()) return;

    state_stats.apply_patch(active_gesture_patch, Clock::now(), Forward, true);

    const auto merged_gesture = action::merge_gesture(active_gesture);
    active_gesture.clear();

    const auto merged_gesture_size = merged_gesture.size();
    // Apply context-dependent transformations to actions with large data members to compress them before committing them to the gesture history.
    const auto active_gesture_compressed = merged_gesture | transform([this, merged_gesture_size](const auto &action) -> Action {
        const auto id = action::get_id(action);
        if (id == action::id<Actions::set_diff_index> && merged_gesture_size == 1) {
            const auto new_diff_index = std::get<Actions::set_diff_index>(action).diff_index;
            if (new_diff_index == gesture_begin_diff_index - 1) return undo{};
            else if (new_diff_index == gesture_begin_diff_index + 1) return redo{};
        }
        return action;
    }) | views::filter([this](const auto &action) {
        // Filter out any resulting `diff_index` actions that don't actually result in a `diff_index` change.
        return action::get_id(action) != action::id<Actions::set_diff_index> || std::get<Actions::set_diff_index>(action).diff_index != gesture_begin_diff_index;
    }) | to<const Gesture>;
    if (!active_gesture_compressed.empty()) gestures.emplace_back(active_gesture_compressed);

    gesture_begin_diff_index = diff_index;
    if (active_gesture_patch.empty()) return;
    if (active_gesture_compressed.empty()) throw std::runtime_error("Non-empty state-diff resulting from an empty compressed gesture!");

    while (int(diffs.size()) > diff_index + 1) diffs.pop_back(); // TODO use an undo _tree_ and keep this history
    diffs.push_back({active_gesture_patch, json::diff(sj, gesture_begin_state_json), Clock::now()});
    diff_index = int(diffs.size()) - 1;
    gesture_begin_diff_index = diff_index;
    gesture_begin_state_json = sj;
    active_gesture_patch.clear();
}

void Context::on_patch(const Action &action, const JsonPatch &patch) {
    active_gesture.emplace_back(action);
    active_gesture_patch = json::diff(gesture_begin_state_json, sj);

    state_stats.apply_patch(patch, Clock::now(), Forward, false);
    for (const auto &patch_op: patch) on_set_value(patch_op.path);
    s.Audio.update_process();
}

void Context::set_diff_index(int new_diff_index) {
    if (new_diff_index == diff_index || new_diff_index < -1 || new_diff_index >= int(diffs.size())) return;

    active_gesture.emplace_back(Actions::set_diff_index{new_diff_index});

    const auto direction = new_diff_index > diff_index ? Forward : Reverse;

    while (diff_index != new_diff_index) {
        const auto &diff = diffs[direction == Reverse ? diff_index-- : ++diff_index];
        const auto &patch = direction == Reverse ? diff.Reverse : diff.Forward;
        state_json = gesture_begin_state_json = state_json.patch(patch);
        state = state_json;
        state_stats.apply_patch(patch, diff.Time, direction, true);
        for (const auto &patch_op: patch) on_set_value(patch_op.path);
    }
    s.Audio.update_process();
}

void Context::increment_diff_index(int diff_index_delta) {
    if (!active_gesture_patch.empty()) finalize_gesture(); // Make sure any pending actions/diffs are committed. _This can change `diff_index`!_
    set_diff_index(diff_index + diff_index_delta);
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

    const auto &project = json::from_msgpack(FileIO::read(path));
    if (format == StateFormat) {
        state_json = gesture_begin_state_json = project;
        state = state_json.get<State>();

        update_ui_context(UIContextFlags_ImGuiSettings | UIContextFlags_ImGuiStyle | UIContextFlags_ImPlotStyle);
        update_faust_context();
    } else if (format == DiffFormat) {
        open_project(EmptyProjectPath); // todo wasteful - need a `set_project_file` method or somesuch to avoid redoing other `open_project` side-effects.

        diffs = project["diffs"];
        int new_diff_index = project["diff_index"];
        on_action(Actions::set_diff_index{new_diff_index});
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
    if (current_project_path.has_value() &&
        fs::equivalent(path, current_project_path.value()) &&
        !action_allowed(action::id<save_current_project>))
        return false;

    const auto format = get_project_format(path);
    if (format == None) return false; // TODO log

    finalize_gesture(); // Make sure any pending actions/diffs are committed.
    if (FileIO::write(path, json::to_msgpack(get_project_json(format)))) {
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
    return FileIO::write(PreferencesPath, json::to_msgpack(preferences));
}
