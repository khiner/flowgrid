#pragma once

#include "action_tree.h"

struct Context {
    const State &state = _state; // Read-only public state
    const State &s = state; // Convenient shorthand
    ActionTree actions;

    void on_action(Action &);
private:
    State _state{};
    void update(Action); // State is only updated via `context.on_action(action)`
};

/**
 * Declare a full name & convenient shorthand for the global `Context` and `State` instances.
 * _These are instantiated in `main.cpp`._
*/
extern Context context, &c;
extern const State &state, &s;

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
