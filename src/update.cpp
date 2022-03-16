#include "state.h"
#include "action.h"
#include "visitor.h"

using namespace action;

// Using the same basic pattern as [`lager`](https://sinusoid.es/lager/architecture.html#reducer).
// TODO Only copy the state to edit when updates need to happen atomically across linked members for logical consistency.
State update(State s, Action action) {
    std::visit(
        visitor{
            [&](toggle_demo_window) { s.windows.demo.show = !s.windows.demo.show; },
            [&](toggle_audio_muted) { s.audio.muted = !s.audio.muted; },
            [&](set_clear_color a) { s.colors.clear = a.color; },
            [&](set_audio_thread_running a) { s.audio.running = a.running; },
        },
        action
    );
    return s;
}
