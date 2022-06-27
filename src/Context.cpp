#include "Context.h"

#include "faust/dsp/llvm-dsp.h"
#include "UI/StatefulFaustUi.h"
#include "File.h"
#include "Audio.h"
#include "ImGuiFileDialog.h"
//#include "generator/libfaust.h" // For the C++ backend

// Used to initialize the static Faust buffer.
// This is the highest `max_frame_count` value I've seen coming into the output audio callback, using a sample rate of 96kHz.
// If it needs bumping up, bump away!
static const int MAX_EXPECTED_FRAME_COUNT = 2048;

struct FaustBuffers {
    const int num_frames = MAX_EXPECTED_FRAME_COUNT;
    const int num_input_channels;
    const int num_output_channels;
    float **input;
    float **output;

    FaustBuffers(int num_input_channels, int num_output_channels) :
        num_input_channels(num_input_channels), num_output_channels(num_output_channels) {
        input = new float *[num_input_channels];
        output = new float *[num_output_channels];
        for (int i = 0; i < num_input_channels; i++) { input[i] = new float[MAX_EXPECTED_FRAME_COUNT]; }
        for (int i = 0; i < num_output_channels; i++) { output[i] = new float[MAX_EXPECTED_FRAME_COUNT]; }
    }

    ~FaustBuffers() {
        for (int i = 0; i < num_input_channels; i++) { delete[] input[i]; }
        for (int i = 0; i < num_output_channels; i++) { delete[] output[i]; }
        delete[] input;
        delete[] output;
    }
};

struct FaustContext {
    int num_inputs{0}, num_outputs{0};
    llvm_dsp_factory *dsp_factory;
    dsp *dsp = nullptr;
    std::unique_ptr<FaustBuffers> buffers;

    FaustContext(const string &code, int sample_rate, string &error_msg) {
        int argc = 0;
        const char **argv = new const char *[8];
        argv[argc++] = "-I";
        argv[argc++] = fs::relative("../lib/faust/libraries").c_str();
        // Consider additional args: "-vec", "-vs", "128", "-dfs"

        const int optimize_level = -1;
        dsp_factory = createDSPFactoryFromString("FlowGrid", code, argc, argv, "", error_msg, optimize_level);

        for (int i = 0; i < argc; i++) argv[i] = nullptr;

        if (dsp_factory && s.audio.faust.error.empty()) {
            dsp = dsp_factory->createDSPInstance();
            dsp->init(sample_rate);
        }
        num_inputs = dsp ? dsp->getNumInputs() : 0;
        num_outputs = dsp ? dsp->getNumOutputs() : 0;
        buffers = std::make_unique<FaustBuffers>(num_inputs, num_outputs);
    }

    ~FaustContext() {
        if (!dsp) return;

        delete dsp;
        dsp = nullptr;
        deleteDSPFactory(dsp_factory);
    }

    void compute(int frame_count) const;
    FAUSTFLOAT get_sample(int channel, int frame) const;
};

void FaustContext::compute(int frame_count) const {
    if (buffers) {
        if (frame_count > buffers->num_frames) {
            std::cerr << "The output stream buffer only has " << buffers->num_frames
                      << " frames, which is smaller than the libsoundio callback buffer size of " << frame_count << "." << std::endl
                      << "(Increase `AudioContext.MAX_EXPECTED_FRAME_COUNT`.)" << std::endl;
            exit(1);
        }
        if (dsp) dsp->compute(frame_count, buffers->input, buffers->output);
    }
    // TODO log warning
}

FAUSTFLOAT FaustContext::get_sample(int channel, int frame) const {
    if (!buffers || !dsp) return 0;
    return buffers->output[std::min(channel, buffers->num_output_channels - 1)][frame];
}

std::unique_ptr<FaustContext> faust;
void Context::compute_frames(int frame_count) const { // NOLINT(readability-convert-member-functions-to-static)
    if (faust) faust->compute(frame_count);
}

FAUSTFLOAT Context::get_sample(int channel, int frame) const {
    return !faust || state.audio.settings.muted ? 0 : faust->get_sample(channel, frame);
}

Context::Context() : derived_state(_state), state_json(_state) {
    if (fs::exists(preferences_path)) {
        preferences = json::from_msgpack(File::read(preferences_path));
    } else {
        write_preferences_file();
    }
}

static const fs::path empty_project_path = "empty" + ExtensionForProjectFormat.at(StateFormat);
static const fs::path default_project_path = "default" + ExtensionForProjectFormat.at(StateFormat);

bool Context::is_user_project_path(const fs::path &path) {
    return !fs::equivalent(path, empty_project_path) && !fs::equivalent(path, default_project_path);
}

json Context::get_project_json(const ProjectFormat format) const {
    if (format == StateFormat) return state_json;
    return diffs;
}

bool Context::project_has_changes() const { return current_diff_index != current_project_saved_action_index; }

bool Context::save_empty_project() { return save_project(empty_project_path); }

bool Context::clear_preferences() {
    preferences.recently_opened_paths.clear();
    return write_preferences_file();
}

void Context::set_state_json(const json &new_state_json) {
    clear_undo();

    state_json = new_state_json;
    _state = state_json.get<State>();
    derived_state = DerivedState(_state);

    update_ui_context(UiContextFlags_ImGuiSettings | UiContextFlags_ImGuiStyle | UiContextFlags_ImPlotStyle);
    update_faust_context();
}

void Context::set_diffs_json(const json &new_diffs_json) {
    open_project(empty_project_path);
    clear_undo();

    diffs = new_diffs_json;
    while (current_diff_index < int(diffs.size() - 1)) {
        apply_diff(++current_diff_index);
    }
}

void Context::enqueue_action(const Action &a) {
    queued_actions.push(a);
}

void Context::run_queued_actions() {
    while (!queued_actions.empty()) {
        on_action(queued_actions.front());
        queued_actions.pop();
    }
    if (!gesturing && !gesture_action_names.empty()) finalize_gesture();
}

bool Context::action_allowed(const ActionID action_id) const {
    switch (action_id) {
        case action::id<undo>: return current_diff_index >= 0;
        case action::id<redo>: return current_diff_index < (int) diffs.size() - 1;
        case action::id<actions::open_default_project>: return fs::exists(default_project_path);
        case action::id<actions::save_project>:
        case action::id<actions::show_save_project_dialog>:
        case action::id<actions::save_default_project>: return project_has_changes();
        case action::id<actions::save_current_project>: return current_project_path.has_value() && project_has_changes();

        case action::id<actions::open_file_dialog>: return !s.file.dialog.visible;
        case action::id<actions::close_file_dialog>: return s.file.dialog.visible;
        default: return true;
    }
}
bool Context::action_allowed(const Action &action) const { return action_allowed(action::get_id(action)); }

// TODO Implement
//  ```cpp
//  std::pair<Diff, Diff> {forward_diff, reverse_diff} = json::bidirectional_diff(old_state_json, new_state_json);
//  ```
//  https://github.com/nlohmann/json/discussions/3396#discussioncomment-2513010

void Context::update_ui_context(UiContextFlags flags) {
    if (flags == UiContextFlags_None) return;

    if (flags & UiContextFlags_ImGuiSettings) s.imgui_settings.populate_context(ui->imgui_context);
    if (flags & UiContextFlags_ImGuiStyle) ui->imgui_context->Style = s.style.imgui;
    if (flags & UiContextFlags_ImPlotStyle) {
        ImPlot::BustItemCache();
        ui->implot_context->Style = s.style.implot;
    }
}

void Context::update_faust_context() {
    has_new_faust_code = true;

    faust = std::make_unique<FaustContext>(s.audio.faust.code, s.audio.settings.sample_rate, _state.audio.faust.error);
    if (faust->dsp) {
        StatefulFaustUI faust_ui;
        faust->dsp->buildUserInterface(&faust_ui);
//                faust->dsp->metadata(&faust_ui); // version/author/licence/etc
//                _s.audio.faust.json = faust_ui.
    } else {
//                _s.audio.faust.json = "";
    }
}

bool audio_running = false;

void Context::update_processes() {
    if (audio_running != s.processes.audio.running) {
        if (s.processes.audio.running) threads.audio_thread = std::thread(audio);
        else threads.audio_thread.join();
        audio_running = s.processes.audio.running;
    }
}

void Context::clear_undo() {
    current_diff_index = -1;
    diffs.clear();
    gesture_action_names.clear();
    gesturing = false;
    state_stats = {};
}

// StateStats

void StateStats::on_json_patch(const JsonPatch &patch, TimePoint time, Direction direction) {
    most_recent_update_paths = {};
    for (const JsonPatchOp &patch_op: patch) {
        // For add/remove ops, the thing being updated is the _parent_.
        const string &changed_path = patch_op.op == Add || patch_op.op == Remove ?
                                     patch_op.path.substr(0, patch_op.path.find_last_of('/')) :
                                     patch_op.path;
        on_json_patch_op(changed_path, time, direction);
        most_recent_update_paths.emplace_back(changed_path);
    }
}

void StateStats::on_json_patch_op(const string &path, TimePoint time, Direction direction) {
    if (direction == Forward) {
        auto &update_times = update_times_for_state_path[path];
        update_times.emplace_back(time);
    } else {
        auto &update_times = update_times_for_state_path.at(path);
        update_times.pop_back();
        if (update_times.empty()) update_times_for_state_path.erase(path);
    }
    path_update_frequency_plottable = create_path_update_frequency_plottable();
    const auto &num_updates = path_update_frequency_plottable.values;
    max_num_updates = num_updates.empty() ? 0 : *std::max_element(num_updates.begin(), num_updates.end());
}

DerivedState::DerivedState(const State &_state) : style(_state.style) {
    window_visible = _state.all_windows_const | ranges::views::transform([](const auto &window_ref) {
        const auto &window = window_ref.get();
        return std::pair<string, bool>(window.name, window.visible);
    }) | ranges::to<std::map<string, bool>>();
}

// Convert `string` to char array, removing first character of the path, which is a '/'.
const char *convert_path(const string &str) {
    char *pc = new char[str.size()];
    std::strcpy(pc, string{str.begin() + 1, str.end()}.c_str());
    return pc;
}

StateStats::Plottable StateStats::create_path_update_frequency_plottable() {
    std::vector<string> paths;
    std::vector<ImU64> values;
    for (const auto &[path, action_times]: update_times_for_state_path) {
        paths.push_back(path);
        values.push_back(action_times.size());
    }

    std::vector<const char *> labels;
    std::transform(paths.begin(), paths.end(), std::back_inserter(labels), convert_path);

    return {labels, values};
}

// Private methods

void Context::on_action(const Action &action) {
    if (!action_allowed(action)) return; // safeguard against actions running in an invalid state

    std::visit(visitor{
        // Handle actions that don't directly update state.
        [&](const undo &) { apply_diff(current_diff_index--, Direction::Reverse); },
        [&](const redo &) { apply_diff(++current_diff_index, Direction::Forward); },

        [&](const actions::open_project &a) { open_project(a.path); },
        [&](const open_empty_project &) { open_project(empty_project_path); },
        [&](const open_default_project &) { open_project(default_project_path); },

        [&](const actions::save_project &a) { save_project(a.path); },
        [&](const save_default_project &) { save_project(default_project_path); },
        [&](const actions::save_current_project &) { save_project(current_project_path.value()); },

        // Remaining actions have a direct effect on the application state.
        [&](const auto &) { update(action); }
    }, action);
}

/**
 * Inspired by [`lager`](https://sinusoid.es/lager/architecture.html#reducer),
 * but only the action-visitor pattern remains.
 *
 * When updates need to happen atomically across linked members for logical consistency,
 * make working copies as needed. Otherwise, modify the (single, global) state directly, in-place.
 */
void Context::update(const Action &action) {
    gesture_action_names.emplace(action::get_name(action));

    auto &_s = _state; // Convenient shorthand for the mutable state that doesn't conflict with the global `s` instance
    std::visit(visitor{
        [&](const show_open_project_dialog &) { _s.file.dialog = {"Choose file", AllProjectExtensionsDelimited, "."}; },
        [&](const show_save_project_dialog &) { _s.file.dialog = {"Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}; },
        [&](const show_open_faust_file_dialog &) { _s.file.dialog = {"Choose file", FaustDspFileExtension, "."}; },
        [&](const show_save_faust_file_dialog &) { _s.file.dialog = {"Choose file", FaustDspFileExtension, ".", "my_dsp", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}; },

        [&](const open_file_dialog &a) {
            _s.file.dialog = a.dialog;
            _s.file.dialog.visible = true;
        },
        [&](const close_file_dialog &) { _s.file.dialog.visible = false; },

        // Setting `imgui_settings` does not require a `c.update_ui_context` on the action,
        // since the action will be initiated by ImGui itself, whereas the style
        // editors don't update the ImGui/ImPlot contexts themselves.
        [&](const set_imgui_settings &a) { _s.imgui_settings = a.settings; },
        [&](const set_imgui_style &a) {
            _s.style.imgui = a.imgui_style;
            c.update_ui_context(UiContextFlags_ImGuiStyle);
        },
        [&](const set_implot_style &a) {
            _s.style.implot = a.implot_style;
            c.update_ui_context(UiContextFlags_ImPlotStyle);
        },
        [&](const set_flowgrid_style &a) { _s.style.flowgrid = a.flowgrid_style; },

        [&](const close_window &a) { _s.named(a.name).visible = false; },
        [&](const toggle_window &a) { _s.named(a.name).visible = !s.named(a.name).visible; },

        [&](const toggle_state_viewer_auto_select &) { _s.state.viewer.auto_select = !s.state.viewer.auto_select; },
        [&](const set_state_viewer_label_mode &a) { _s.state.viewer.label_mode = a.label_mode; },

        // Audio
        [&](const save_faust_dsp_file &a) { File::write(a.path, s.audio.faust.code); },
        [&](const open_faust_dsp_file &a) { _s.audio.faust.code = File::read(a.path); },
        [&](const set_faust_code &a) { _s.audio.faust.code = a.text; },
        [&](const toggle_audio_muted &) { _s.audio.settings.muted = !s.audio.settings.muted; },
        [&](const set_audio_sample_rate &a) { _s.audio.settings.sample_rate = a.sample_rate; },

        [&](const set_audio_running &a) { _s.processes.audio.running = a.running; },
        [&](const toggle_audio_running &) { _s.processes.audio.running = !s.processes.audio.running; },
        [&](const set_ui_running &a) { _s.processes.ui.running = a.running; },

        [&](const close_application &) {
            _s.processes.ui.running = false;
            _s.processes.audio.running = false;
        },

        [&](const auto &) {}, // All actions that don't directly update state (e.g. undo/redo & open/load-project)
    }, action);
}

void Context::finalize_gesture() {
    auto old_json_state = state_json;
    state_json = s;
    const JsonPatch patch = json::diff(old_json_state, state_json);
    if (patch.empty()) return;

    while (int(diffs.size()) > current_diff_index + 1) diffs.pop_back();

    const JsonPatch reverse_patch = json::diff(state_json, old_json_state);
    const BidirectionalStateDiff diff{gesture_action_names, patch, reverse_patch, Clock::now()};
    gesture_action_names.clear();

    diffs.emplace_back(diff);
    current_diff_index = int(diffs.size()) - 1;

    on_json_diff(diff, Forward, true);
}

void Context::apply_diff(const int index, const Direction direction) {
    const auto &diff = diffs[index];
    const auto &patch = direction == Forward ? diff.forward_patch : diff.reverse_patch;

    state_json = state_json.patch(patch);
    _state = state_json.get<State>();
    derived_state = DerivedState(_state);

    on_json_diff(diff, direction, false);
}

void Context::on_json_diff(const BidirectionalStateDiff &diff, Direction direction, bool ui_initiated) {
    // These state-paths trigger side effects when changed
    const static auto imgui_settings_path = StatePath(s.imgui_settings);
    const static auto imgui_style_path = StatePath(s.style.imgui);
    const static auto implot_style_path = StatePath(s.style.implot);
    const static auto faust_code_path = StatePath(s.audio.faust.code);

    const auto &patch = direction == Forward ? diff.forward_patch : diff.reverse_patch;
    state_stats.on_json_patch(patch, diff.system_time, direction);

    if (!ui_initiated) {
        // Only need to update the UI context for undo/redo. UI-initiated actions already take care of this.
        UiContextFlags update_ui_flags = UiContextFlags_None;
        for (const auto &patch_op: patch) {
            const auto &path = patch_op.path;
            if (path.rfind(imgui_settings_path, 0) == 0) update_ui_flags |= UiContextFlags_ImGuiSettings;
            else if (path.rfind(imgui_style_path, 0) == 0) update_ui_flags |= UiContextFlags_ImGuiStyle;
            else if (path.rfind(implot_style_path, 0) == 0) update_ui_flags |= UiContextFlags_ImPlotStyle;
        }
        update_ui_context(update_ui_flags);
    }

    for (const auto &patch_op: patch) {
        const auto &path = patch_op.path;
        if (path == faust_code_path) update_faust_context();
    }

    update_processes();
}

void Context::open_project(const fs::path &path) {
    const auto project_format = ProjectFormatForExtension.at(path.extension());
    if (project_format == None) return; // TODO log

    const json &project_json = json::from_msgpack(File::read(path));
    if (project_format == StateFormat) set_state_json(project_json);
    else set_diffs_json(project_json);

    if (is_user_project_path(path)) {
        set_current_project_path(path);
    } else {
        current_project_path.reset();
        current_project_saved_action_index = -1;
    }
}

ProjectFormat get_project_format(const fs::path &path) {
    const string &ext = path.extension();
    return ProjectFormatForExtension.contains(ext) ? ProjectFormatForExtension.at(ext) : None;
}

bool Context::save_project(const fs::path &path) {
    if (current_project_path.has_value() && fs::equivalent(path, current_project_path.value()) && !action_allowed(action::id<save_current_project>)) return false;

    const ProjectFormat format = get_project_format(path);
    if (format == None) return false; // TODO log

    if (File::write(path, json::to_msgpack(get_project_json(format)))) {
        if (is_user_project_path(path)) {
            set_current_project_path(path);
        }
        return true;
    }
    return false;
}

void Context::set_current_project_path(const fs::path &path) {
    current_project_path = path;
    current_project_saved_action_index = current_diff_index;
    preferences.recently_opened_paths.remove(path);
    preferences.recently_opened_paths.emplace_front(path);
    write_preferences_file();
}

bool Context::write_preferences_file() const {
    return File::write(preferences_path, json::to_msgpack(preferences));
}
