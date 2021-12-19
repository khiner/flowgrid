#pragma once

#include "state.h"
#include "action.h"
#include "visitor.h"

// Using the same basic pattern as [`lager`](https://sinusoid.es/lager/architecture.html#reducer).
State update(State state, action action) {
    return std::visit(
            visitor{
                    [&](Action::set_clear_color a) {
                        // This example is not actually used, since ImGui updates the variable directly in `draw`.
                        state.colors.clear = a.color;
                        return state;
                    },
            },
            action);
}
