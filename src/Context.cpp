#include "Context.h"

#include "faust/dsp/llvm-dsp.h"
#include "UI/StatefulFaustUi.h"
#include "File.h"
#include "Audio.h"
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
    return !faust || s.audio.settings.muted ? 0 : faust->get_sample(channel, frame);
}

Context::Context() : state_json(state), previous_state_json(state) {
    if (fs::exists(PreferencesPath)) {
        preferences = json::from_msgpack(File::read(PreferencesPath));
    } else {
        write_preferences_file();
    }
}

bool Context::is_user_project_path(const fs::path &path) {
    // Using relative path to avoid error: `filesystem error: in equivalent: Operation not supported`
    return !fs::equivalent(fs::relative(path), EmptyProjectPath) && !fs::equivalent(fs::relative(path), DefaultProjectPath);
}

json Context::get_project_json(const ProjectFormat format) const {
    if (format == StateFormat) return sj;
    return diffs;
}

bool Context::project_has_changes() const { return current_diff_index != current_project_saved_action_index; }

bool Context::save_empty_project() { return save_project(EmptyProjectPath); }

bool Context::clear_preferences() {
    preferences.recently_opened_paths.clear();
    return write_preferences_file();
}

void Context::set_state_json(const json &new_state_json) {
    clear_undo();

    state_json = previous_state_json = new_state_json;
    state = state_json.get<State>();

    update_ui_context(UiContextFlags_ImGuiSettings | UiContextFlags_ImGuiStyle | UiContextFlags_ImPlotStyle);
    update_faust_context();
}

void Context::set_diffs_json(const json &new_diffs_json) {
    open_project(EmptyProjectPath);
    clear_undo();

    diffs = new_diffs_json;
    while (current_diff_index < int(diffs.size() - 1)) {
        apply_diff(++current_diff_index);
    }
}

void Context::enqueue_action(const Action &a) {
    queued_actions.push(a);
}

void Context::run_queued_actions(bool merge_gesture) {
    while (!queued_actions.empty()) {
        on_action(queued_actions.front());
        queued_actions.pop();
    }
    if (!gesturing && !gesture_action_names.empty()) finalize_gesture(merge_gesture);
}

bool Context::action_allowed(const ActionID action_id) const {
    switch (action_id) {
        case action::id<undo>: return current_diff_index >= 0;
        case action::id<redo>: return current_diff_index < (int) diffs.size() - 1;
        case action::id<actions::open_default_project>: return fs::exists(DefaultProjectPath);
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

    faust = std::make_unique<FaustContext>(s.audio.faust.code, s.audio.settings.sample_rate, state.audio.faust.error);
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
        [&](const open_empty_project &) { open_project(EmptyProjectPath); },
        [&](const open_default_project &) { open_project(DefaultProjectPath); },

        [&](const actions::save_project &a) { save_project(a.path); },
        [&](const save_default_project &) { save_project(DefaultProjectPath); },
        [&](const actions::save_current_project &) { save_project(current_project_path.value()); },

        // Remaining actions have a direct effect on the application state.
        [&](const auto &) { update(action); }
    }, action);
}

void Context::update(const Action &action) {
    // Update state. Keep JSON & struct versions of state in sync.
    if (std::holds_alternative<set_value>(action)) {
        const auto &a = std::get<set_value>(action);
        state_json[JsonPath(a.state_path)] = a.value;
        state = state_json;
    } else {
        state.update(action);
        state_json = state;
    }

    // Execute side effects.
    gesture_action_names.emplace(action::get_name(action));
    std::visit(visitor{
        // Setting `imgui_settings` does not require a `c.update_ui_context` on the action,
        // since the action will be initiated by ImGui itself,
        // whereas the style editors don't update the ImGui/ImPlot contexts themselves.
        [&](const set_imgui_color_style &) { update_ui_context(UiContextFlags_ImGuiStyle); },
        [&](const set_implot_color_style &) { update_ui_context(UiContextFlags_ImPlotStyle); },
        [&](const save_faust_file &a) { File::write(a.path, s.audio.faust.code); },

        [&](const set_value &a) { on_set_value(a.state_path); },

        [&](const auto &) {}, // All actions without side effects (only state updates)
    }, action);
}

void Context::finalize_gesture(bool merge) {
    const auto gesture_names_copy = gesture_action_names;
    gesture_action_names.clear();

    const bool should_merge = merge && !diffs.empty();
    if (should_merge && int(diffs.size()) != current_diff_index + 1) return; // Only allow merges for new gestures at the end of the undo chain.

    const json &compare_with_state_json = should_merge ? previous_state_json.patch(diffs.back().reverse_patch) : previous_state_json;
    previous_state_json = s;

    const JsonPatch patch = json::diff(compare_with_state_json, sj);
    if (patch.empty()) return;

    const JsonPatch reverse_patch = json::diff(sj, compare_with_state_json);
    BidirectionalStateDiff diff{gesture_names_copy, patch, reverse_patch, Clock::now()};

    if (should_merge) {
        const auto last_diff = diffs.back();
        diffs.pop_back();
        diff.action_names.insert(last_diff.action_names.begin(), last_diff.action_names.end());
        on_json_diff(last_diff, Reverse);
    }

    // If we're not already at the end of the undo stack, wind it back.
    // TODO use an undo _tree_ and keep this history
    while (int(diffs.size()) > current_diff_index + 1) diffs.pop_back();

    diffs.emplace_back(diff);
    current_diff_index = int(diffs.size()) - 1;

    on_json_diff(diff, Forward);
}

void Context::apply_diff(const int index, const Direction direction) {
    const auto &diff = diffs[index];
    const auto &patch = direction == Forward ? diff.forward_patch : diff.reverse_patch;

    state_json = previous_state_json = state_json.patch(patch);
    state = state_json.get<State>();

    on_json_diff(diff, direction);
}

void Context::on_set_value(const string &path) {
    if (path.rfind("/imgui_settings", 0) == 0) update_ui_context(UiContextFlags_ImGuiSettings); // TODO only when not ui-initiated
    else if (path.rfind("/style/imgui", 0) == 0) update_ui_context(UiContextFlags_ImGuiStyle);
    else if (path.rfind("/style/implot", 0) == 0) update_ui_context(UiContextFlags_ImPlotStyle);
    else if (path == "/audio/faust/code") update_faust_context();
}

void Context::on_json_diff(const BidirectionalStateDiff &diff, Direction direction) {
    const auto &patch = direction == Forward ? diff.forward_patch : diff.reverse_patch;
    state_stats.on_json_patch(patch, diff.system_time, direction);
    for (const auto &patch_op: patch) on_set_value(patch_op.path);
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
    return File::write(PreferencesPath, json::to_msgpack(preferences));
}
