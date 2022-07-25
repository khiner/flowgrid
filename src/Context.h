#pragma once

#include <iostream>
#include <list>
#include <queue>
#include <thread>

#include "Action.h"

struct Preferences {
    std::list<fs::path> recently_opened_paths;
};

JsonType(Preferences, recently_opened_paths)

namespace views = ranges::views;
using std::string;

namespace FlowGrid {}
namespace fg = FlowGrid;
using Action = action::Action;

enum ProjectFormat {
    None,
    StateFormat,
    DiffFormat,
};

const std::map<ProjectFormat, string> ExtensionForProjectFormat{
    {StateFormat, ".fls"},
    {DiffFormat,  ".fld"},
};
const std::map<string, ProjectFormat> ProjectFormatForExtension{
    {ExtensionForProjectFormat.at(StateFormat), StateFormat},
    {ExtensionForProjectFormat.at(DiffFormat),  DiffFormat},
};

static const std::set<string> AllProjectExtensions = {".fls", ".fld"};
static const string AllProjectExtensionsDelimited = AllProjectExtensions | views::join(',') | ranges::to<std::string>();
static const string PreferencesFileExtension = ".flp";
static const string FaustDspFileExtension = ".dsp";

static const fs::path InternalPath = ".flowgrid";
static const fs::path EmptyProjectPath = InternalPath / ("empty" + ExtensionForProjectFormat.at(StateFormat));
static const fs::path DefaultProjectPath = InternalPath / ("default" + ExtensionForProjectFormat.at(StateFormat));
static const fs::path PreferencesPath = InternalPath / ("preferences" + PreferencesFileExtension);

struct State : StateData, Drawable {
    State() = default;

    // Don't copy/assign reference members!
    explicit State(const StateData &other) : StateData(other) {}

    State &operator=(const State &other) {
        StateData::operator=(other);
        return *this;
    }

    void draw() const override;
    void update(const Action &); // State is only updated via `context.on_action(action)`

    std::vector<std::reference_wrapper<WindowData>> all_windows{
        state_viewer, memory_editor, path_update_frequency,
        style, demo, metrics, tools,
        audio.settings, audio.faust.editor, audio.faust.log
    };

    using WindowNamed = std::map<string, std::reference_wrapper<WindowData>>;

    WindowNamed window_named = all_windows | views::transform([](const auto &window_ref) {
        return std::pair<string, std::reference_wrapper<WindowData>>(window_ref.get().name, window_ref);
    }) | ranges::to<WindowNamed>();
};

struct UiContext {
    UiContext(ImGuiContext *imgui_context, ImPlotContext *implot_context) : imgui_context(imgui_context), implot_context(implot_context) {}

    ImGuiContext *imgui_context;
    ImPlotContext *implot_context;
};

using UiContextFlags = int;
enum UiContextFlags_ {
    UiContextFlags_None = 0,
    UiContextFlags_ImGuiSettings = 1 << 0,
    UiContextFlags_ImGuiStyle = 1 << 1,
    UiContextFlags_ImPlotStyle = 1 << 2,
};

enum Direction { Forward, Reverse };

struct Threads {
    std::thread audio_thread;
};

struct StateStats {
    struct Plottable {
        std::vector<const char *> labels;
        std::vector<ImU64> values;
    };

    std::map<string, std::vector<TimePoint>> update_times_for_state_path{};
    Plottable path_update_frequency_plottable;
    ImU32 max_num_updates{0};
    std::vector<string> most_recent_update_paths{};

    void on_json_patch(const JsonPatch &patch, TimePoint time, Direction direction);

private:
    void on_json_patch_op(const string &path, TimePoint time, Direction direction);
    Plottable create_path_update_frequency_plottable();
};

struct Context {
    Context();
    ~Context() = default;

    bool save_empty_project();

    json get_project_json(ProjectFormat format = StateFormat) const;
    static bool is_user_project_path(const fs::path &);
    bool project_has_changes() const;

    bool clear_preferences();

    // Takes care of all side effects needed to put the app into the provided application state json.
    // This function can be run at any time, but it's not thread-safe.
    // Running it on anything but the UI thread could cause correctness issues or event crash with e.g. a NPE during a concurrent read.
    // This is especially the case when assigning to `state_json`, which is not an atomic operation like assigning to `_state` is.
    void set_state_json(const json &);
    void set_diffs_json(const json &);

    void enqueue_action(const Action &);
    // If `merge_gesture = true`, the gesture diff will be merged with the previous one when it's finalized.
    void run_queued_actions(bool merge_gesture = false);
    size_t num_queued_actions() { return queued_actions.size(); }

    bool action_allowed(ActionID) const;
    bool action_allowed(const Action &) const;

    void clear_undo();

    // Audio
    void compute_frames(int frame_count) const;
    float get_sample(int channel, int frame) const;

    void update_ui_context(UiContextFlags flags);
    void update_faust_context();
    void update_processes();

    Preferences preferences;

/**md
 * # Writing directly to state
 *
 * For now at least, feel free to write directly to state for events that are internally generated,
 * but with potential side effects that can affect the audiovisual output. (`internal_action` as a thing?)
 *
 * By using the more verbose `c._state`, rather than the conventional `s` global const-reference,
 * you signal that you know what you're doing, and that this event should not be considered an undoable
 * user action.
 *
 * ## Example
 *
 * An example use case is setting an error message.
 *
 * An error message should be stored in `s`, since that object should fully specify the UI.
 * However, there shouldn't also be a `set_faust_error_message` `Action`, since that isn't
 * something a user should ever be allowed to change, as it would violate the reasonable
 * expectation that a variable called `faust_error_message` would only be populated with
 * _actual_ Faust errors.
 *
 * Also, we don't want setting the error messages to pollute the undo tree with its own action.
 */
    State state{};
//    diff_match_patch<string> dmp;
    UiContext *ui{};
    StateStats state_stats;

    /**
     This is a placeholder for the main in-memory data structure for action history.
     Undo should have similar functionality to [Vim's undotree](https://github.com/mbbill/undotree/blob/master/autoload/undotree.vim)
       - Consider the Hash Array Mapped Trie (HAMT) data structure for state, diff, and/or actions (fast keyed access and fast-ish updates,
         exploiting the state's natural tree structure.
       - Probably just copy (with MIT copyright notice as required)
         [this header](https://github.com/chaelim/HAMT/tree/bf7621d1ef3dfe63214db6a9293ce019fde99bcf/include),
         and modify to taste.
    */
    std::vector<BidirectionalStateDiff> diffs;
    int current_diff_index = -1;

    std::optional<fs::path> current_project_path;
    int current_project_saved_action_index = -1;

    ImFont *defaultFont{};
    ImFont *fixedWidthFont{};

    bool gesturing{};
    bool has_new_faust_code{};

    // Read-only public shorthand state references:
    const State &s = state;
    const json &sj = state_json;
private:
    void on_action(const Action &); // Immediately execute the action
    void update(const Action &); // State is only updated via `context.on_action(action)`
    void finalize_gesture(bool merge = false); // If `merge = true`, this gesture's diff is rolled into the previous diff.
    void apply_diff(int index, Direction direction = Forward);
    void on_json_diff(const BidirectionalStateDiff &diff, Direction direction);
    void on_set_value(const JsonPath &path);

    void open_project(const fs::path &);
    bool save_project(const fs::path &);
    void set_current_project_path(const fs::path &path);
    bool write_preferences_file() const;

    Threads threads;
    std::queue<const Action> queued_actions;
    std::set<string> gesture_action_names; // TODO change to `gesture_actions` (IDs)
    json state_json, previous_state_json; // `state_json` always reflects `state`. `previous_state_json` is only updated on gesture-end (for diff calculation).
};

/**
 * Declare a full name & convenient shorthand for the global `Context` & `State` instances.
 * _These are instantiated in `main.cpp`._
*/
extern Context context, &c;
extern const State &s;
extern const json &sj;

// This is the main action-queue method.
// Providing `flush = true` will run all enqueued actions (including this one) and finalize any open gesture.
// This is useful for running multiple actions in a single frame, without grouping them into a single gesture.
inline bool q(Action &&a, bool flush = false) {
    // Bailing on async action consumer for now, to avoid issues with concurrent state reads/writes, esp for json.
    // Commit dc81a9ff07e1b8e61ae6613d49183abb292abafc gets rid of the queue
    // return queue.enqueue(a);

    c.enqueue_action(a); // Actions within a single UI frame are queued up and flushed at the end of the frame.
    if (flush) c.run_queued_actions(); // ... unless the `flush` flag is provided, in which case we just do it now.
    return true;
}

/**md
# Usage

```cpp
// Declare an explicitly typed local reference to the global `Context` instance `c`
Context &local_context = c;
// ...and one for global `State` instance `s` inside the global context:
State &local_state = c.s;

// Or just access the (read-only) `state` members directly
Audio audio = s.audio;
```
 */
