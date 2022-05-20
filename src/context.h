#pragma once

// Import common libraries once in this widely-shared header
#include <iostream>

#include "state.h"
#include "action.h"
#include "blockingconcurrentqueue.h"
#include "diff_match_patch.h"

// TODO The time definitions/units here look nice https://stackoverflow.com/a/14391562, might want to adopt
using namespace std::chrono_literals;
using Clock = std::chrono::system_clock;
using SystemTime = std::chrono::time_point<Clock, std::chrono::nanoseconds>;
using Nanos = std::chrono::nanoseconds;

struct RenderContext;
struct UiContext {
    UiContext(ImGuiContext *imgui_context, ImPlotContext *implot_context) : imgui_context(imgui_context), implot_context(implot_context) {}

    ImGuiContext *imgui_context;
    ImPlotContext *implot_context;
};

using namespace nlohmann; // json

enum Direction { Forward, Reverse };

struct StateDiff {
    json json_diff;
};

// One issue with this data structure is that forward & reverse diffs both redundantly store the same json path(s).
struct BidirectionalStateDiff {
    StateDiff forward;
    StateDiff reverse;
    SystemTime system_time;
};

struct Config {
    std::string app_root;
    std::string faust_libraries_path{};
};

struct StateStats {
    struct Plottable {
        std::vector<const char *> labels;
        std::vector<ImU64> values;
    };

    std::map<std::string, std::vector<SystemTime>> update_times_for_state_path{};
    Plottable path_update_frequency_plottable;
    ImU64 max_num_updates{0};
    std::vector<std::string> most_recent_update_paths{};

    // `patch` conforms to the [JSON patch](http://jsonpatch.com/) spec.
    void on_json_diff(const json &diff, SystemTime time, Direction direction) {
        most_recent_update_paths = {};
        for (auto &patch: diff) {
            const std::string path = patch["path"];
            const std::string op = patch["op"];
            // For add/remove ops, the thing being updated is the _parent_.
            const std::string changed_path = op == "add" || op == "remove" ? path.substr(0, path.find_last_of('/')) : path;
            on_json_patch(changed_path, time, direction);
            most_recent_update_paths.emplace_back(changed_path);
        }
    }

private:
    // Convert `std::string` to char array, removing first character of the path, which is a '/'.
    static const char *convert_path(const std::string &str) {
        char *pc = new char[str.size()];
        std::strcpy(pc, std::string{str.begin() + 1, str.end()}.c_str());
        return pc;
    }

    void on_json_patch(const std::string &path, SystemTime time, Direction direction) {
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

    Plottable create_path_update_frequency_plottable() {
        std::vector<std::string> paths;
        std::vector<ImU64> values;
        for (const auto &[path, action_times]: update_times_for_state_path) {
            paths.push_back(path);
            values.push_back(action_times.size());
        }

        std::vector<const char *> labels;
        std::transform(paths.begin(), paths.end(), std::back_inserter(labels), convert_path);

        return {labels, values};
    }
};

struct Context {
private:
    void update(const Action &); // State is only updated via `context.on_action(action)`
    void apply_diff(int index, Direction direction);
    void finalize_gesture();
public:
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
 * Also, we don't want error messages to pollute the undo tree.
 */
    StateStats state_stats;
    State _state{};
    diff_match_patch<std::string> dmp;
    UiContext *ui;

    const State &state = _state; // Read-only public state
    const State &s = state; // Convenient shorthand
    State ui_s{}; // Separate copy of the state that can be modified by the UI directly

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
    int current_action_index = -1;
    json state_json;

    ImFont *defaultFont{};
    ImFont *fixedWidthFont{};

    bool in_gesture{};

    Context();
    ~Context() = default;

    void on_action(const Action &);

    void start_gesture() { in_gesture = true; }
    void end_gesture() {
        in_gesture = false;
        finalize_gesture();
    }
    bool can_undo() const { return current_action_index >= 0; }
    bool can_redo() const { return current_action_index < (int) diffs.size() - 1; }

    void clear_undo() {
        current_action_index = -1;
        diffs.clear();
    }

    // Audio
    void compute_frames(int frame_count) const;
    float get_sample(int channel, int frame) const;

    void reset_from_state_json() {
        // Overwrite all the primary state variables.
        _state = state_json.get<State>();
        ui_s = _state; // Update the UI-copy of the state to reflect.

        // Other housekeeping side-effects:
        // TODO Consider grouping these into a the constructor of a new `struct DerivedFullState` (or somesuch) member,
        //  and do this atomically with a single assignment.
        state_stats = {};
    }
};

/**
 * Declare a full name & convenient shorthand for the global `Context` and `State` instances.
 * _These are instantiated in `main.cpp`._
*/
extern Context context, &c;
extern const State &state, &s;
extern State &ui_s;

using namespace moodycamel; // Has `ConcurrentQueue` & `BlockingConcurrentQueue`

/**
 * The action queue is available for use anywhere in the application.
 * However, the singular usage of `queue.wait_dequeue` is in `main.cpp`.
 * In fact, that's currently the only place in the application that anything but `enqueue` is called on `queue`.
 *
 * For convenience, a shorthand `q(action)` function is provided.
 * By convention, there are no usages of `queue.enqueue(action)` in the code; the application only uses `q` to enqueue actions.
 * enqueue an action into the main application action consumer loop.
 */
extern BlockingConcurrentQueue<Action> queue;

// False positive unused function from CLion.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
static inline bool q(Action &&item) { return queue.enqueue(item); }
#pragma clang diagnostic pop

extern Config config;

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
