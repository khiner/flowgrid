#pragma once

#include <iostream>

#include "action.h"
#include "state_renderers/render_json.h"
#include "visitor.h"

using namespace action;

struct Context {
    const State &state = _state; // Read-only public state
    const State &s = state; // Convenient shorthand

    void update(Action);
private:
    State _state{};
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
