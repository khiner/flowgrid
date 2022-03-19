#pragma once

#include "nlohmann/json.hpp"
#include "state.h"
#include "action.h"

using namespace nlohmann;

struct Context {
private:
    State _state{};
    void update(Action); // State is only updated via `context.on_action(action)`
    void apply_diff(const json &diff);
public:
    struct ActionDiff {
        Action action;
        json forward_diff;
        json reverse_diff;
    };

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
    std::vector<ActionDiff> actions;
    int current_action_index = -1;
    json json_state;

    Context();

    void on_action(Action &);
    bool can_undo() const {
        return current_action_index >= 0;
    }
    bool can_redo() const {
        return current_action_index < (int) actions.size() - 1;
    }
};

/**
 * Declare a full name & convenient shorthand for the global `Context` and `State` instances.
 * _These are instantiated in `main.cpp`._
*/
extern Context context, &c;
extern const State &state, &s;
extern State &ui_s;

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
