#include "Context.h"

#include "faust/dsp/llvm-dsp.h"
#include "UI/StatefulFaustUI.h"
#include "Audio.h"

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
        write_preferences();
    }
}

bool Context::is_user_project_path(const fs::path &path) {
    // Using relative path to avoid error: `filesystem error: in equivalent: Operation not supported`
    return !fs::equivalent(fs::relative(path), EmptyProjectPath) && !fs::equivalent(fs::relative(path), DefaultProjectPath);
}

bool Context::project_has_changes() const { return diff_index != current_project_saved_diff_index; }

bool Context::save_empty_project() { return save_project(EmptyProjectPath); }

bool Context::clear_preferences() {
    preferences.recently_opened_paths.clear();
    return write_preferences();
}

json Context::get_project_json(const ProjectFormat format) const {
    switch (format) {
        case None: return nullptr;
        case StateFormat: return sj;
        case DiffFormat:
            return {{"diffs",      diffs},
                    {"diff_index", diff_index}};
        case ActionFormat: return gestures;
    }
}

void Context::enqueue_action(const Action &a) {
    queued_actions.push(a);
}

static const int gesture_frames = 50; // todo time-based rather than frame-based
void Context::run_queued_actions(bool force_finalize_gesture) {
    if (!queued_actions.empty()) gesture_frames_remaining = gesture_frames;
    while (!queued_actions.empty()) {
        on_action(queued_actions.front());
        queued_actions.pop();
    }
    if ((!gesturing && gesture_frames_remaining == 0) || force_finalize_gesture) finalize_gesture();
    gesture_frames_remaining = std::max(0, gesture_frames_remaining - 1);
}

bool Context::action_allowed(const ActionID action_id) const {
    switch (action_id) {
        case action::id<Actions::undo>: return !active_gesture.empty() || diff_index >= 0;
        case action::id<Actions::redo>: return diff_index < (int) diffs.size() - 1;
        case action::id<Actions::open_default_project>: return fs::exists(DefaultProjectPath);
        case action::id<Actions::save_project>:
        case action::id<Actions::show_save_project_dialog>:
        case action::id<Actions::save_default_project>: return project_has_changes();
        case action::id<Actions::save_current_project>: return current_project_path.has_value() && project_has_changes();
        case action::id<Actions::open_file_dialog>: return !s.file.dialog.visible;
        case action::id<Actions::close_file_dialog>: return s.file.dialog.visible;
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

    if (flags & UIContextFlags_ImGuiSettings) s.imgui_settings.populate_context(ui->imgui_context);
    if (flags & UIContextFlags_ImGuiStyle) ui->imgui_context->Style = s.style.imgui;
    if (flags & UIContextFlags_ImPlotStyle) {
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

void Context::undo() { apply_diff(diff_index--, Direction::Reverse); }
void Context::redo() { apply_diff(++diff_index, Direction::Forward); }

void Context::clear() {
    diff_index = current_project_saved_diff_index = -1;
    current_project_path.reset();
    diffs.clear();
    gestures.clear();
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

    active_gesture.emplace_back(action);

    std::visit(visitor{
        // Handle actions that don't directly update state.
        [&](const Actions::undo &) {
            if (!active_gesture.empty()) finalize_gesture();
            undo();
        },
        [&](const Actions::redo &) { redo(); },

        [&](const Actions::open_project &a) { open_project(a.path); },
        [&](const open_empty_project &) { open_project(EmptyProjectPath); },
        [&](const open_default_project &) { open_project(DefaultProjectPath); },

        [&](const Actions::save_project &a) { save_project(a.path); },
        [&](const save_default_project &) { save_project(DefaultProjectPath); },
        [&](const Actions::save_current_project &) { save_project(current_project_path.value()); },

        // Remaining actions have a direct effect on the application state.
        [&](const auto &) { update(action); },
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
    std::visit(visitor{
        // Setting `imgui_settings` does not require a `c.update_ui_context` on the action,
        // since the action will be initiated by ImGui itself,
        // whereas the style editors don't update the ImGui/ImPlot contexts themselves.
        [&](const set_imgui_color_style &) { update_ui_context(UIContextFlags_ImGuiStyle); },
        [&](const set_implot_color_style &) { update_ui_context(UIContextFlags_ImPlotStyle); },
        [&](const save_faust_file &a) { File::write(a.path, s.audio.faust.code); },

        [&](const set_value &a) { on_set_value(JsonPath(a.state_path)); },

        [&](const auto &) {}, // All actions without side effects (only state updates)
    }, action);
}

void Context::finalize_gesture() {
    if (active_gesture.empty()) return;

    const auto &compressed_gesture = action::compress_gesture(active_gesture);
    active_gesture.clear();

    const JsonPatch patch = json::diff(previous_state_json, sj);

    // If state hasn't changed, we don't even append the gesture to history.
    // Gesture history is there to replay the session, and we don't want to re-execute any gestures with non-state-affecting side effects.
    // E.g. a gesture with only a project-save action has no effect on state;
    // It _only_ has the non-state-affecting side effect of writing to disk, and we don't want to re-execute this side effect in playback.
    // This also catches any cases where the compressed gesture (list of actions) is _not_ empty, but it really should be.
    // E.g. compressing `undo,undo,undo,redo,redo,redo` will currently result in `undo,undo,redo,redo`,
    // since compression only considers two actions at a time for simplicity.
    // todo this doesn't currently guarantee actions with non-state side-effect won't get saved to history! They could sneak into gestures with other state effects.
    if (patch.empty()) return;
    else if (compressed_gesture.empty()) throw std::runtime_error("Non-empty state-diff resulting from an empty compressed gesture!");

    const JsonPatch reverse_patch = json::diff(sj, previous_state_json);
    previous_state_json = sj;
    // If we're not already at the end of the undo stack, wind it back. TODO use an undo _tree_ and keep this history
    while (int(diffs.size()) > diff_index + 1) diffs.pop_back();
    gestures.emplace_back(compressed_gesture);
    diffs.push_back({patch, reverse_patch, Clock::now()});
    diff_index = int(diffs.size()) - 1;

    on_json_diff(diffs.back(), Forward);
}

void Context::apply_diff(const int index, const Direction direction) {
    const auto &diff = diffs[index];
    const auto &patch = direction == Forward ? diff.forward_patch : diff.reverse_patch;

    state_json = state_json.patch(patch);
    state = state_json.get<State>();

    on_json_diff(diff, direction);
}

void Context::on_set_value(const JsonPath &path) {
    const auto &path_str = path.to_string();
    if (path_str.rfind(s.imgui_settings.path, 0) == 0) update_ui_context(UIContextFlags_ImGuiSettings); // TODO only when not ui-initiated
    else if (path_str.rfind(s.style.imgui.path, 0) == 0) update_ui_context(UIContextFlags_ImGuiStyle); // TODO add `starts_with` method to nlohmann/json?
    else if (path_str.rfind(s.style.implot.path, 0) == 0) update_ui_context(UIContextFlags_ImPlotStyle);
    else if (path == s.audio.faust.path / "code") update_faust_context();
}

void Context::on_json_diff(const BidirectionalStateDiff &diff, Direction direction) {
    const auto &patch = direction == Forward ? diff.forward_patch : diff.reverse_patch;
    state_stats.on_json_patch(patch, diff.system_time, direction);
    for (const auto &patch_op: patch) on_set_value(JsonPath(patch_op.path));
    update_processes();
}

ProjectFormat get_project_format(const fs::path &path) {
    const string &ext = path.extension();
    return ProjectFormatForExtension.contains(ext) ? ProjectFormatForExtension.at(ext) : None;
}

void Context::open_project(const fs::path &path) {
    const auto format = get_project_format(path);
    if (format == None) return; // TODO log

    clear();

    const auto &project = json::from_msgpack(File::read(path));
    if (format == StateFormat) {
        state_json = previous_state_json = project;
        state = state_json.get<State>();

        update_ui_context(UIContextFlags_ImGuiSettings | UIContextFlags_ImGuiStyle | UIContextFlags_ImPlotStyle);
        update_faust_context();
    } else if (format == DiffFormat) {
        open_project(EmptyProjectPath); // todo wasteful - need a `set_project_file` method or somesuch to avoid redoing other `open_project` side-effects.

        diffs = project["diffs"];
        int new_diff_index = project["diff_index"];
        while (diff_index < new_diff_index) redo();
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

    finalize_gesture(); // Just in case we're in the middle of a gesture, make sure any pending actions/diffs are committed.
    if (File::write(path, json::to_msgpack(get_project_json(format)))) {
        if (is_user_project_path(path)) set_current_project_path(path);
        return true;
    }
    return false;
}

void Context::set_current_project_path(const fs::path &path) {
    current_project_path = path;
    current_project_saved_diff_index = diff_index;
    preferences.recently_opened_paths.remove(path);
    preferences.recently_opened_paths.emplace_front(path);
    write_preferences();
}

bool Context::write_preferences() const {
    return File::write(PreferencesPath, json::to_msgpack(preferences));
}
