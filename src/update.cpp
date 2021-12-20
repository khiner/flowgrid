#include "state.h"
#include "action.h"
#include "visitor.h"

using namespace action;

// Using the same basic pattern as [`lager`](https://sinusoid.es/lager/architecture.html#reducer).
State update(State s, Action action) {
    std::visit(
        visitor{
            [&](toggle_demo_window) { s.windows.demo.show = !s.windows.demo.show; },
            [&](toggle_sine_wave) { s.sine.on = !s.sine.on; },
            [&](set_clear_color a) { s.colors.clear = a.color; },
            [&](set_audio_engine_running a) { s.audio.running = a.running; },
            [&](set_sine_frequency a) { s.sine.frequency = a.frequency; },
            [&](set_sine_amplitude a) { s.sine.amplitude = a.amplitude; },
        },
        action
    );
    return s;
}
