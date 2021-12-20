#include "state.h"
#include "action.h"
#include "visitor.h"

using namespace action;

// Using the same basic pattern as [`lager`](https://sinusoid.es/lager/architecture.html#reducer).
State update(State state, Action action) {
    std::visit(
        visitor{
            [&](toggle_demo_window) { state.show_demo_window = !state.show_demo_window; },
            [&](toggle_sine_wave) { state.sine.on = !state.sine.on; },
            [&](set_clear_color a) { state.colors.clear = a.color; },
            [&](set_audio_engine_running a) { state.audio_engine_running = a.running; },
            [&](set_sine_frequency a) { state.sine.frequency = a.frequency; },
            [&](set_sine_amplitude a) { state.sine.amplitude = a.amplitude; },
        },
        action
    );
    return state;
}
