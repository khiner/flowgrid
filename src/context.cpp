#include "context.h"

#include "faust/dsp/llvm-dsp.h"
#include "stateful_faust_ui.h"
#include "file_helpers.h"
#include "audio.h"
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

std::unique_ptr<FaustContext> faust;

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

void Context::compute_frames(int frame_count) const { // NOLINT(readability-convert-member-functions-to-static)
    if (faust) faust->compute(frame_count);
}

FAUSTFLOAT Context::get_sample(int channel, int frame) const {
    return !faust || state.audio.settings.muted ? 0 : faust->get_sample(channel, frame);
}

Context::Context() : state_json(_state) {
    if (fs::exists(preferences_path)) {
        preferences = json::from_msgpack(read_file(preferences_path));
    } else {
        write_preferences_file();
    }
}

static const fs::path default_project_path = "default_project.flo";
bool Context::is_default_project_path(const fs::path &path) {
    return fs::equivalent(path, default_project_path);
}

void Context::open_project(const fs::path &path) {
    set_state_json(json::from_msgpack(read_file(path)));

    if (!is_default_project_path(path)) {
        set_current_project_path(path);
    } else {
        current_project_path.reset();
        current_project_saved_action_index = -1;
    }
}
void Context::open_default_project() {
    open_project(default_project_path);
}

bool Context::can_save_project() const {
    return current_project_path.has_value() && current_action_index != current_project_saved_action_index;
}
bool Context::save_project(const fs::path &path) {
    if (current_project_path.has_value() && fs::equivalent(path, current_project_path.value()) && !can_save_project()) return false;

    if (write_file(path, json::to_msgpack(state_json))) {
        if (!is_default_project_path(path)) {
            set_current_project_path(path);
        }
        return true;
    }
    return false;
}
bool Context::save_default_project() {
    return save_project(default_project_path);
}
bool Context::save_current_project() {
    return can_save_project() && save_project(current_project_path.value());
}

bool Context::clear_preferences() {
    preferences.recently_opened_paths.clear();
    return write_preferences_file();
}

void Context::set_state_json(const json &new_state_json) {
    state_json = new_state_json;
    _state = state_json.get<State>();
    ui_s = _state; // Update the UI-copy of the state to reflect.

    // TODO Consider grouping these into a the constructor of a new `struct DerivedFullState` (or somesuch) member,
    //  and do this atomically with a single assignment.
    clear_undo();

    update_ui_context(UiContextFlags_ImGuiSettings | UiContextFlags_ImGuiStyle | UiContextFlags_ImPlotStyle);
}

void Context::enqueue_action(const Action &a) {
    queued_actions.push(a);
}

void Context::run_queued_actions() {
    while (!queued_actions.empty()) {
        on_action(queued_actions.front());
        queued_actions.pop();
    }
}

void Context::on_action(const Action &action) {
    gesture_action_names.emplace(get_action_name(action));
    std::visit(visitor{
        [&](undo) {
            if (can_undo()) apply_diff(current_action_index--, Direction::Reverse);
        },
        [&](redo) {
            if (can_redo()) apply_diff(++current_action_index, Direction::Forward);
        },
        [&](const action::open_project &a) { open_project(a.path); },
        [&](const action::save_project &a) { save_project(a.path); },
        [&](action::open_default_project) { open_default_project(); },
        [&](action::save_default_project) { save_default_project(); },
        [&](action::save_current_project) { save_current_project(); },
        [&](auto) { // other action
            update(action);
            if (!gesturing) end_gesture();
        }
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
    auto &_s = _state; // Convenient shorthand for the mutable state that doesn't conflict with the global `s` instance
    std::visit(visitor{
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

        [&](const toggle_window &a) { _s.named(a.name).visible = !s.named(a.name).visible; },

        [&](const toggle_state_viewer_auto_select &) { _s.state.viewer.auto_select = !s.state.viewer.auto_select; },
        [&](const set_state_viewer_label_mode &a) { _s.state.viewer.label_mode = a.label_mode; },

        // Audio
        [&](const set_faust_code &a) {
            _s.audio.faust.code = a.text;

            // TODO move this side-effect to post-processing
            faust = std::make_unique<FaustContext>(s.audio.faust.code, s.audio.settings.sample_rate, _s.audio.faust.error);
            if (faust->dsp) {
                StatefulFaustUI faust_ui;
                faust->dsp->buildUserInterface(&faust_ui);
//                faust->dsp->metadata(&faust_ui); // version/author/licence/etc
//                _s.audio.faust.json = faust_ui.
            } else {
//                _s.audio.faust.json = "";
            }
        },
        [&](toggle_audio_muted) { _s.audio.settings.muted = !s.audio.settings.muted; },
        [&](const set_audio_sample_rate &a) { _s.audio.settings.sample_rate = a.sample_rate; },

        [&](set_audio_running a) { _s.processes.audio.running = a.running; },
        [&](toggle_audio_running) { _s.processes.audio.running = !s.processes.audio.running; },
        [&](set_ui_running a) { _s.processes.ui.running = a.running; },

        [&](close_application) {
            _s.processes.ui.running = false;
            _s.processes.audio.running = false;
        },

        [&](auto) {}, // All actions that don't directly update state (e.g. undo/redo & open/load-project)
    }, action);
}

void Context::apply_diff(const int action_index, const Direction direction) {
    const auto &diff = diffs[action_index];
    const auto &patch = direction == Forward ? diff.forward_patch : diff.reverse_patch;

    state_json = state_json.patch(patch);
    _state = state_json.get<State>();
    ui_s = _state; // Update the UI-copy of the state to reflect.

    on_json_diff(diff, direction, false);
}

// TODO Implement
//  ```cpp
//  std::pair<Diff, Diff> {forward_diff, reverse_diff} = json::bidirectional_diff(old_state_json, new_state_json);
//  ```
//  https://github.com/nlohmann/json/discussions/3396#discussioncomment-2513010
void Context::end_gesture() {
    const auto action_names = gesture_action_names; // Make a copy so we can clear.
    gesturing = false;
    gesture_action_names.clear();

    auto old_json_state = state_json;
    state_json = s;
    const JsonPatch patch = json::diff(old_json_state, state_json);
    if (patch.empty()) return;

    while (int(diffs.size()) > current_action_index + 1) diffs.pop_back();

    const JsonPatch reverse_patch = json::diff(state_json, old_json_state);
    const BidirectionalStateDiff diff{action_names, patch, reverse_patch, Clock::now()};
    diffs.emplace_back(diff);
    current_action_index = int(diffs.size()) - 1;

    on_json_diff(diff, Forward, true);
}

void Context::set_current_project_path(const fs::path &path) {
    current_project_path = path;
    current_project_saved_action_index = current_action_index;
    preferences.recently_opened_paths.remove(path);
    preferences.recently_opened_paths.emplace_front(path);
    write_preferences_file();
}

bool Context::write_preferences_file() {
    return write_file(preferences_path, json::to_msgpack(preferences));
}

void Context::update_ui_context(UiContextFlags flags) {
    if (flags == UiContextFlags_None) return;

    if (flags & UiContextFlags_ImGuiSettings) s.imgui_settings.populate_context(ui->imgui_context);
    if (flags & UiContextFlags_ImGuiStyle) ui->imgui_context->Style = s.style.imgui;
    if (flags & UiContextFlags_ImPlotStyle) {
        ImPlot::BustItemCache();
        ui->implot_context->Style = s.style.implot;
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

// Private

void Context::on_json_diff(const BidirectionalStateDiff &diff, Direction direction, bool ui_initiated) {
    const auto &patch = direction == Forward ? diff.forward_patch : diff.reverse_patch;
    state_stats.on_json_patch(patch, diff.system_time, direction);

    if (!ui_initiated) {
        // Only need to update the UI context for undo/redo. UI-initiated actions already take care of this.
        UiContextFlags update_ui_flags = UiContextFlags_None;
        for (const auto &patch_op: patch) {
            const auto &path = patch_op.path;
            // TODO really would like these paths as constants, but don't want to define and maintain them manually.
            //  Need to find a way to create a mapping between `State::...` c++ code references and paths (as a `std::filesystem::path`).
            if (path.rfind("/imgui_settings", 0) == 0) update_ui_flags |= UiContextFlags_ImGuiSettings;
            else if (path.rfind("/style/imgui", 0) == 0) update_ui_flags |= UiContextFlags_ImGuiStyle;
            else if (path.rfind("/style/implot", 0) == 0) update_ui_flags |= UiContextFlags_ImPlotStyle;
        }
        update_ui_context(update_ui_flags);
    }

    update_processes();
}
