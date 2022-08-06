#include "Context.h"

#include "faust/dsp/llvm-dsp.h"
#include "UI/StatefulFaustUI.h"

// Used to initialize the static Faust buffer.
// This is the highest `max_frame_count` value I've seen coming into the output audio callback, using a sample rate of 96kHz
// AND switching between different sample rates, which seems to make for high peak frame sizes at the transition frame.
// If it needs bumping up, bump away!
static const int MAX_EXPECTED_FRAME_COUNT = 8192;

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

Context::Context() : state_json(state), gesture_begin_state_json(state) {
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
        case StateFormat: return sj;
        case DiffFormat:
            return {{"diffs",      diffs},
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
    gesture_time_remaining_sec = std::max(0.0f, s.application_settings.GestureDurationSec - fsec(Clock::now() - gesture_start_time).count());
    if (!(is_widget_gesturing || gesture_time_remaining_sec > 0) || force_finalize_gesture) finalize_gesture();
}

bool Context::action_allowed(const ActionID action_id, bool user_initiated) const {
    switch (action_id) {
        case action::id<undo>: return !active_gesture_patch.empty() || diff_index >= 0;
        case action::id<redo>: return diff_index < (int) diffs.size() - 1;
        case action::id<Actions::open_default_project>: return user_initiated && fs::exists(DefaultProjectPath);
        case action::id<Actions::save_project>:
        case action::id<Actions::show_save_project_dialog>:
        case action::id<Actions::save_default_project>: return user_initiated && project_has_changes();
        case action::id<Actions::save_current_project>: return user_initiated && current_project_path.has_value() && project_has_changes();
        case action::id<Actions::open_file_dialog>: return user_initiated && !s.file.dialog.visible;
        case action::id<Actions::close_file_dialog>: return user_initiated && s.file.dialog.visible;
        default: return true;
    }
}
bool Context::action_allowed(const Action &action, bool user_initiated) const { return action_allowed(action::get_id(action), user_initiated); }

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

void Context::clear() {
    diff_index = current_project_saved_diff_index = -1;
    current_project_path.reset();
    diffs.clear();
    gestures.clear();
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
    path_update_frequency = create_path_update_frequency_plottable();
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

void Context::on_action(const Action &action, bool user_initiated) {
    // todo if not allowed, still track in gesture history with some kind of 'not-applied' flag?
    //  problem I'm trying to solve: can save e.g. save-project action in history, for posterity,
    //  and when loading an `.fga` file, show it in history but don't replay it.
    if (!action_allowed(action, user_initiated)) return; // Safeguard against actions running in an invalid state.

    std::visit(visitor{
        // Handle actions that don't directly update state.
        [&](const Actions::open_project &a) { open_project(a.path); },
        [&](const open_empty_project &) { open_project(EmptyProjectPath); },
        [&](const open_default_project &) { open_project(DefaultProjectPath); },

        [&](const Actions::save_project &a) { save_project(a.path); },
        [&](const save_default_project &) { save_project(DefaultProjectPath); },
        [&](const Actions::save_current_project &) { save_project(current_project_path.value()); },
        [&](const save_faust_file &a) { File::write(a.path, s.audio.faust.code); },

        // Remaining actions have a direct effect on the application state.
        // Keep JSON & struct versions of state in sync.
        [&](const undo &) {
            if (!active_gesture_patch.empty()) finalize_gesture(); // Make sure any pending actions/diffs are committed. todo this prevents merging redo,undo
            const auto &diff = diffs[diff_index--];
            const auto &patch = diff.reverse;
            state_json = gesture_begin_state_json = state_json.patch(patch);
            state = state_json.get<State>();
            on_diff(diff, Reverse, true);
        },
        [&](const redo &) {
            const auto &diff = diffs[++diff_index];
            const auto &patch = diff.forward;
            state_json = gesture_begin_state_json = state_json.patch(patch);
            state = state_json.get<State>();
            on_diff(diff, Forward, true);
        },
        [&](const set_value &a) {
            const auto before_json = state_json;
            state_json[a.path] = a.value;
            state = state_json;
            on_patch(json::diff(before_json, state_json));
        },
        [&](const toggle_value &a) {
            const auto before_json = state_json;
            state_json[a.path] = !state_json[a.path];
            state = state_json;
            on_patch(json::diff(before_json, state_json));
        },
        [&](const auto &a) {
            const auto before_json = state_json;
            state.update(a);
            state_json = state;
            on_patch(json::diff(before_json, state_json));
        },
    }, action);

    active_gesture.emplace_back(action);
    active_gesture_patch = json::diff(gesture_begin_state_json, sj);
}

void Context::finalize_gesture() {
    if (active_gesture.empty()) return;

    const auto active_gesture_compressed = action::compress_gesture(active_gesture);
    active_gesture.clear();
    state_stats.apply_patch(active_gesture_patch, Clock::now(), Forward, true);
    if (!active_gesture_compressed.empty()) gestures.emplace_back(active_gesture_compressed);

    if (active_gesture_patch.empty()) return;
    if (active_gesture_compressed.empty()) throw std::runtime_error("Non-empty state-diff resulting from an empty compressed gesture!");

    while (int(diffs.size()) > diff_index + 1) diffs.pop_back(); // TODO use an undo _tree_ and keep this history
    diffs.push_back({active_gesture_patch, json::diff(sj, gesture_begin_state_json), Clock::now()});
    diff_index = int(diffs.size()) - 1;
    gesture_begin_state_json = sj;
    active_gesture_patch.clear();
}

void Context::on_set_value(const JsonPath &path) {
    const auto &path_str = path.to_string();

    // Setting `imgui_settings` does not require a `c.update_ui_context` on the action,
    // since the action will be initiated by ImGui itself,
    // whereas the style editors don't update the ImGui/ImPlot contexts themselves.
    if (path_str.rfind(s.imgui_settings.path.to_string(), 0) == 0) update_ui_context(UIContextFlags_ImGuiSettings); // TODO only when not ui-initiated
    else if (path_str.rfind(s.style.imgui.path.to_string(), 0) == 0) update_ui_context(UIContextFlags_ImGuiStyle); // TODO add `starts_with` method to nlohmann/json?
    else if (path_str.rfind(s.style.implot.path.to_string(), 0) == 0) update_ui_context(UIContextFlags_ImPlotStyle);
    else if (path == s.audio.faust.path / "code") update_faust_context();
}

void Context::on_diff(const BidirectionalStateDiff &diff, Direction direction, bool is_full_gesture) {
    const auto &patch = direction == Forward ? diff.forward : diff.reverse;
    state_stats.apply_patch(patch, diff.time, direction, is_full_gesture);
    for (const auto &patch_op: patch) on_set_value(patch_op.path);
    s.audio.update_process();
}

void Context::on_patch(const JsonPatch &patch) {
    on_diff({patch, {}, Clock::now()}, Forward, false);
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
        state_json = gesture_begin_state_json = project;
        state = state_json.get<State>();

        update_ui_context(UIContextFlags_ImGuiSettings | UIContextFlags_ImGuiStyle | UIContextFlags_ImPlotStyle);
        update_faust_context();
    } else if (format == DiffFormat) {
        open_project(EmptyProjectPath); // todo wasteful - need a `set_project_file` method or somesuch to avoid redoing other `open_project` side-effects.

        diffs = project["diffs"];
        int new_diff_index = project["diff_index"];
        while (diff_index < new_diff_index) on_action(redo{}, false);
    } else if (format == ActionFormat) {
        open_project(EmptyProjectPath);

        const Gestures project_gestures = project;
        for (const auto &gesture: project_gestures) {
            for (const auto &action: gesture) on_action(action, false);
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
