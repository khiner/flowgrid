#include "context.h"

/**
 * Inspired by [`lager`](https://sinusoid.es/lager/architecture.html#reducer),
 * but only the action-visitor pattern remains.
 *
 * When updates need to happen atomically across linked members for logical consistency,
 * make working copies as needed. Otherwise, modify the (single, global) state directly, in-place.
 */
void Context::update(Action action) {
    State &s = _state;
    std::visit(
        visitor{
            [&](toggle_demo_window) { s.windows.demo.show = !s.windows.demo.show; },
            [&](toggle_audio_muted) { s.audio.muted = !s.audio.muted; },
            [&](set_clear_color a) { s.colors.clear = a.color; },
            [&](set_audio_thread_running a) { s.audio.running = a.running; },
            [&](set_action_consumer_running a) { s.action_consumer.running = a.running; },
        },
        action
    );
}
