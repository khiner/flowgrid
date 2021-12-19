#pragma once

#include "state.h"
#include "action.h"
#include "visitor.h"

// Using the same basic pattern as [`lager`](https://sinusoid.es/lager/architecture.html#reducer).
State update(State state, Action action) {
    return std::visit(
            visitor{
                    [&](action::set_clear_color a) {
                        state.colors.clear = a.color;
                        return state;
                    },
                    [&](action::set_audio_engine_running a) {
                        state.audio_engine_running = a.running;
                        return state;
                    }
            },
            action);
}
