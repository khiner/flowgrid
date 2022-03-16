#include "state.h"
#include "action.h"
#include "visitor.h"

using namespace action;

// Using the same basic pattern as [`lager`](https://sinusoid.es/lager/architecture.html#reducer).
// When updates need to happen atomically across linked members for logical consistency,
// make working copies as needed. Otherwise, modify the (single, global) state directly, in-place.
void update(State &s, Action action) {
    std::visit(
        visitor{
            [&](toggle_demo_window) { s.windows.demo.show = !s.windows.demo.show; },
            [&](toggle_audio_muted) { s.audio.muted = !s.audio.muted; },
            [&](set_clear_color a) { s.colors.clear = a.color; },
            [&](set_audio_thread_running a) { s.audio.running = a.running; },
        },
        action
    );
}
