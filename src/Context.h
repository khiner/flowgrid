#pragma once

#include <iostream>
#include <list>
#include <queue>

#include "Action.h"
#include "Helper/File.h"

struct Preferences {
    std::list<fs::path> recently_opened_paths;
};

JsonType(Preferences, recently_opened_paths)

namespace FlowGrid {}
namespace fg = FlowGrid;
using Action = action::Action;

const std::map<ProjectFormat, string> ExtensionForProjectFormat{
    {StateFormat, ".fls"},
    {DiffFormat, ".fld"},
    {ActionFormat, ".fla"},
};

// todo derive from above map
const std::map<string, ProjectFormat> ProjectFormatForExtension{
    {ExtensionForProjectFormat.at(StateFormat), StateFormat},
    {ExtensionForProjectFormat.at(DiffFormat), DiffFormat},
    {ExtensionForProjectFormat.at(ActionFormat), ActionFormat},
};

static const std::set<string> AllProjectExtensions = {".fls", ".fld", ".fla"}; // todo derive from map
static const string AllProjectExtensionsDelimited = AllProjectExtensions | views::join(',') | to<string>;
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
};

using UIContextFlags = int;
enum UIContextFlags_ {
    UIContextFlags_None = 0,
    UIContextFlags_ImGuiSettings = 1 << 0,
    UIContextFlags_ImGuiStyle = 1 << 1,
    UIContextFlags_ImPlotStyle = 1 << 2,
};

enum Direction { Forward, Reverse };

struct StateStats {
    struct Plottable {
        std::vector<const char *> labels;
        std::vector<ImU64> values;
    };

    std::vector<JsonPath> latest_updated_paths{};
    std::map<JsonPath, std::vector<TimePoint>> gesture_update_times_for_path{};
    std::map<JsonPath, std::vector<TimePoint>> committed_update_times_for_path{};
    std::map<JsonPath, TimePoint> latest_update_time_for_path{};
    Plottable path_update_frequency;

    void apply_patch(const JsonPatch &patch, TimePoint time, Direction direction, bool is_gesture);

private:
    Plottable create_path_update_frequency_plottable();
};

struct Context {
    Context();
    ~Context() = default;

    static bool is_user_project_path(const fs::path &);
    bool project_has_changes() const;
    void save_empty_project();

    bool clear_preferences();

    json get_project_json(ProjectFormat format = StateFormat);

    void enqueue_action(const Action &);
    void run_queued_actions(bool force_finalize_gesture = false);
    bool action_allowed(ActionID) const;
    bool action_allowed(const Action &) const;

    void clear();

    // Audio
    void compute_frames(int frame_count) const;
    float get_sample(int channel, int frame) const;

    void update_ui_context(UIContextFlags flags);
    void update_faust_context();

    Preferences preferences;

//    diff_match_patch<string> dmp;
    UIContext *ui{};
    StateStats state_stats;

    Diffs diffs;
    int diff_index = -1;

    Gesture active_gesture; // uncompressed, uncommitted
    Gestures gestures; // compressed, committed gesture history
    JsonPatch active_gesture_patch;

    std::optional<fs::path> current_project_path;
    size_t project_start_gesture_count = gestures.size();

    ImFont *defaultFont{};
    ImFont *fixedWidthFont{};

    bool is_widget_gesturing{};
    bool has_new_faust_code{};
    TimePoint gesture_start_time{};
    float gesture_time_remaining_sec{};

    // Read-only public shorthand state references:
    const State &s = state;
    const json &sj = state_json;

private:
    void on_action(const Action &); // This is the only method that modifies `state`.
    void finalize_gesture();
    void on_patch(const Action &action, const JsonPatch &patch); // Called after every state-changing action
    void set_diff_index(int diff_index);
    void increment_diff_index(int diff_index_delta);
    void on_set_value(const JsonPath &path);

    // Takes care of all side effects needed to put the app into the provided application state json.
    // This function can be run at any time, but it's not thread-safe.
    // Running it on anything but the UI thread could cause correctness issues or event crash with e.g. a NPE during a concurrent read.
    // This is especially the case when assigning to `state_json`, which is not an atomic operation like assigning to `_state` is.
    void open_project(const fs::path &);
    bool save_project(const fs::path &);
    void set_current_project_path(const fs::path &path);
    bool write_preferences() const;

    State state{};
    std::queue<const Action> queued_actions;
    json state_json, gesture_begin_state_json; // `state_json` always reflects `state`. `gesture_begin_state_json` is only updated on gesture-end (for diff calculation).
    int gesture_begin_diff_index = -1;
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

    c.enqueue_action(a); // Actions within a single UI frame are queued up and flushed at the end of the frame (see `main.cpp`).
    if (flush) c.run_queued_actions(true); // ... unless the `flush` flag is provided, in which case we just finalize the gesture now.
    return true;
}

/**md
# Usage

```cpp
State &state = c.s;
Audio audio = s.audio; // Or just access the (read-only) `state` members directly
```
 */
