#pragma once

#include "state.h"
#include "action.h"
#include "visitor.h"

using namespace action;

// Using the same basic pattern as [`lager`](https://sinusoid.es/lager/architecture.html#reducer).
State update(State state, Action action) {
    std::visit(
        visitor{
            [&](set_clear_color a) { state.colors.clear = a.color; },
            [&](set_audio_engine_running a) { state.audio_engine_running = a.running; }
        },
        action
    );
    return state;
}
