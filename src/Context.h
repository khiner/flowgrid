#pragma once

#include <iostream>
#include <queue>
#include <thread>

#include "Preferences.h"
#include "State.h"

struct RenderContext;
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
    State _state{};
//    diff_match_patch<string> dmp;
    UiContext *ui{};
    StateStats state_stats;

    const State &state = _state; // Read-only public state
    const State &s = state; // Convenient shorthand

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
    json state_json;

    std::optional<fs::path> current_project_path;
    int current_project_saved_action_index = -1;

    ImFont *defaultFont{};
    ImFont *fixedWidthFont{};

    bool gesturing{};
    bool has_new_faust_code{};

private:
    void on_action(const Action &); // Immediately execute the action
    void update(const Action &); // State is only updated via `context.on_action(action)`
    void finalize_gesture(bool merge = false); // If `merge = true`, this gesture's diff is rolled into the previous diff.
    void apply_diff(int index, Direction direction = Forward);
    void on_json_diff(const BidirectionalStateDiff &diff, Direction direction);
    void on_set_value(const string &path);

    void open_project(const fs::path &);
    bool save_project(const fs::path &);
    void set_current_project_path(const fs::path &path);
    bool write_preferences_file() const;

    Threads threads;
    std::queue<const Action> queued_actions;
    std::set<string> gesture_action_names; // TODO change to `gesture_actions` (IDs)
};

/**
 * Declare a full name & convenient shorthand for the global `Context` & `State` instances.
 * _These are instantiated in `main.cpp`._
*/
extern Context context, &c;
extern const State &state, &s;

// This is the main action-queue method.
inline bool q(Action &&a) {
    // Bailing on async action consumer for now, to avoid issues with concurrent state reads/writes, esp for json.
    // Commit dc81a9ff07e1b8e61ae6613d49183abb292abafc gets rid of the queue
    // return queue.enqueue(a);

    c.enqueue_action(a); // Actions within a single UI frame are queued up and flushed at the end of the frame.
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
