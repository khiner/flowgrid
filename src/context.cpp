#include <iostream>
#include "context.h"
#include "visitor.h"

#include "faust/dsp/llvm-dsp.h"
#include "stateful_faust_ui.h"
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

    FaustContext(const std::string &code, int sample_rate, std::string &error_msg) {
        int argc = 0;
        const char **argv = new const char *[8];
        argv[argc++] = "-I";
        argv[argc++] = &config.faust_libraries_path[0]; // convert to char*
        // Consider additional args: "-vec", "-vs", "128", "-dfs"

        const int optimize_level = -1;
        dsp_factory = createDSPFactoryFromString("FlowGrid", code, argc, argv, "", error_msg, optimize_level);

        for (int i = 0; i < argc; i++) argv[i] = nullptr;

        if (dsp_factory && s.windows.faust.error.empty()) {
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
    return !faust || state.audio.muted ? 0 : faust->get_sample(channel, frame);
}

std::ostream &operator<<(std::ostream &os, const StateDiff &diff) {
    return (os << "\tJSON diff:\n" << diff.json_diff << "\n\tINI diff:\n" << diff.ini_diff);
}
std::ostream &operator<<(std::ostream &os, const BidirectionalStateDiff &diff) {
    return (os << "Forward:\n" << diff.forward << "\nReverse:\n" << diff.reverse);
}

Context::Context() : state_json(_state) {}

void Context::on_action(const Action &action) {
    if (std::holds_alternative<undo>(action)) {
        if (can_undo()) apply_diff(current_action_index--, Direction::Reverse);
    } else if (std::holds_alternative<redo>(action)) {
        if (can_redo()) apply_diff(++current_action_index, Direction::Forward);
    } else {
        update(action);
        if (!in_gesture) finalize_gesture();
    }
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
    std::visit(
        visitor{
            [&](const set_ini_settings &a) { c.ini_settings = a.settings; },
            [&](const set_imgui_style &a) { _s.style.imgui = a.imgui_style; },
            [&](const set_implot_style &a) { _s.style.implot = a.implot_style; },

            [&](const toggle_window &a) { _s.windows.named(a.name).visible = !s.windows.named(a.name).visible; },

            [&](const set_state_viewer_label_mode &a) { _s.windows.state.viewer.settings.label_mode = a.label_mode; },

            [&](set_ui_running a) { _s.processes.ui.running = a.running; },

            [&](close_application) {
                _s.processes.ui.running = false;
                _s.processes.audio.running = false;
                _s.processes.action_consumer.running = false;
            },

            // Audio
            [&](const set_faust_code &a) {
                c._state.windows.faust.code = a.text;

                faust = std::make_unique<FaustContext>(s.windows.faust.code, s.audio.sample_rate, _s.windows.faust.error);
                if (faust->dsp) {
                    StatefulFaustUI faust_ui;
                    faust->dsp->buildUserInterface(&faust_ui);
//                    faust->dsp->metadata(&faust_ui); // version/author/licence/etc
//                    _s.audio.faust.json = faust_ui.
                } else {
//                    _s.audio.faust.json = "";
                }
            },
            [&](toggle_audio_muted) { _s.audio.muted = !s.audio.muted; },
            [&](set_audio_thread_running a) { _s.processes.audio.running = a.running; },
            [&](toggle_audio_running) { _s.processes.audio.running = !s.processes.audio.running; },
            [&](const set_audio_sample_rate &a) { _s.audio.sample_rate = a.sample_rate; },

            // All actions that don't affect state:
            [&](undo) {},
            [&](redo) {},
        },
        action
    );
}

void Context::apply_diff(const int action_index, const Direction direction) {
    const auto &diff = diffs[action_index];
    const auto &d = direction == Forward ? diff.forward : diff.reverse;

    const auto [new_ini_settings, successes] = dmp.patch_apply(dmp.patch_fromText(d.ini_diff), ini_settings);
    if (!std::all_of(successes.begin(), successes.end(), [](bool v) { return v; })) {
        throw std::runtime_error("Some ini-settings patches were not successfully applied.\nSettings:\n\t" +
            ini_settings + "\nPatch:\n\t" + d.ini_diff + "\nResult:\n\t" + new_ini_settings);
    }

    ini_settings = prev_ini_settings = new_ini_settings;
    state_json = state_json.patch(d.json_diff);
    _state = state_json;
    ui_s = _state; // Update the UI-copy of the state to reflect.

    if (!d.ini_diff.empty()) has_new_ini_settings = true;

    for (auto &jd: d.json_diff) {
        if (std::string(jd["path"]).rfind("/ui/style/implot", 0) == 0) {
            has_new_implot_style = true;
        }
        state_stats.on_path_update(jd["path"], diff.system_time, direction);
    }
}

// TODO Implement
//  ```cpp
//  std::pair<Diff, Diff> {forward_diff, reverse_diff} = json::bidirectional_diff(old_state_json, new_state_json);
//  ```
//  https://github.com/nlohmann/json/discussions/3396#discussioncomment-2513010
void Context::finalize_gesture() {
    auto old_json_state = state_json;
    state_json = s;
    auto json_diff = json::diff(old_json_state, state_json);

    auto old_ini_settings = prev_ini_settings;
    prev_ini_settings = ini_settings;
    auto ini_settings_patches = dmp.patch_make(old_ini_settings, ini_settings);

    if (!json_diff.empty() || !ini_settings_patches.empty()) {
        while (int(diffs.size()) > current_action_index + 1) diffs.pop_back();

        // TODO put diff/patch/text fns in `transformers/bijective`
        auto ini_settings_diff = diff_match_patch<std::string>::patch_toText(ini_settings_patches);
        auto ini_settings_reverse_diff = diff_match_patch<std::string>::patch_toText(dmp.patch_make(ini_settings, old_ini_settings));
        const BidirectionalStateDiff diff{
            {json_diff, ini_settings_diff},
            {json::diff(state_json, old_json_state), ini_settings_reverse_diff},
            std::chrono::system_clock::now(),
        };
        diffs.emplace_back(diff);
        current_action_index = int(diffs.size()) - 1;
        for (auto &jd: json_diff) {
            state_stats.on_path_update(jd["path"], diff.system_time, Forward);
        }
    }
}
