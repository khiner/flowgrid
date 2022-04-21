#pragma once

#include "nlohmann/json.hpp"
#include "state.h"
#include "action.h"
#include "blockingconcurrentqueue.h"
#include "diff_match_patch.h"

using namespace nlohmann; // json

struct ActionDiff {
    json json_diff;
    std::string ini_diff; // string-encoded `diff_match_patch::Patches`
};
struct ActionDiffs {
    ActionDiff forward;
    ActionDiff reverse;
};

struct Config {
    std::string app_root;
    std::string faust_libraries_path{};
};

extern Config config; // Initialized in `main.cpp`

struct Context {
private:
    void update(const Action &); // State is only updated via `context.on_action(action)`
    void apply_diff(const ActionDiff &diff);
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
    State _state{};
    diff_match_patch<std::string> dmp;

    const State &state = _state; // Read-only public state
    const State &s = state; // Convenient shorthand
    State ui_s{}; // Separate copy of the state that can be modified by the UI directly
    std::string ini_settings; // ImGui's ini settings (for UI state) are stored separately
    // Actions change the source-of-truth `ini_settings` in-place.
    // When a gesture is finalized into an undo event, a patch is computed between the previous and current settings.
    std::string prev_ini_settings;
    // Set after an undo/redo that includes an ini_settings change.
    // It's the UI's responsibility to set this to `false` after applying the new state.
    // (It's done this way to avoid an expensive settings-load & long string comparison between
    // ours and ImGui's ini settings on every frame.)
    bool has_new_ini_settings{};

    /**
     This is a placeholder for the main in-memory data structure for action history.
     Undo should have similar functionality to [Vim's undotree](https://github.com/mbbill/undotree/blob/master/autoload/undotree.vim)
       - Consider the Hash Array Mapped Trie (HAMT) data structure for state, diff, and/or actions (fast keyed access and fast-ish updates,
         exploiting the state's natural tree structure.
       - Probably just copy (with MIT copyright notice as required)
         [this header](https://github.com/chaelim/HAMT/tree/bf7621d1ef3dfe63214db6a9293ce019fde99bcf/include),
         and modify to taste.
    */
    std::vector<ActionDiffs> actions;
    int current_action_index = -1;
    json json_state;
    bool in_gesture{};

    Context();

    void on_action(const Action &);

    void start_gesture() { in_gesture = true; }
    void end_gesture() {
        in_gesture = false;
        finalize_gesture();
    }
    bool can_undo() const { return current_action_index >= 0; }
    bool can_redo() const { return current_action_index < (int) actions.size() - 1; }

    void clear_undo() {
        current_action_index = -1;
        actions.clear();
    }

    // Audio
    void compute_frames(int frame_count) const;
    float get_sample(int channel, int frame) const;
};

/**
 * Declare a full name & convenient shorthand for the global `Context` and `State` instances.
 * _These are instantiated in `main.cpp`._
*/
extern Context context, &c;
extern const State &state, &s;
extern State &ui_s;

using namespace moodycamel; // ConcurrentQueue, BlockingConcurrentQueue
extern BlockingConcurrentQueue<Action> q;

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
