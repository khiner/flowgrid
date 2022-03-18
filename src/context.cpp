#include <iostream>
#include "context.h"
#include "transformers/bijective/state2json.h"
#include "visitor.h"

Context::Context() : json_state(state2json(_state)) {}

void Context::on_action(Action &action) {
    update(action);
    auto old_json_state = json_state;
    json_state = state2json(s);
    auto state_diff = json::diff(old_json_state, json_state);
    actions.emplace_back(std::make_pair(action, state_diff));
    current_action_index += 1;
    std::cout << "Action #" << actions.size() << " diff: " << actions.back().second << std::endl;
}

/**
 * Inspired by [`lager`](https://sinusoid.es/lager/architecture.html#reducer),
 * but only the action-visitor pattern remains.
 *
 * When updates need to happen atomically across linked members for logical consistency,
 * make working copies as needed. Otherwise, modify the (single, global) state directly, in-place.
 */
void Context::update(Action action) {
    State &_s = _state; // Convenient shorthand for the mutable state that doesn't conflict with the global `s` instance
    std::visit(
        visitor{
            [&](undo) {},
            [&](redo) {},
            [&](toggle_demo_window) { _s.ui.windows.demo.show = !s.ui.windows.demo.show; },
            [&](toggle_audio_muted) { _s.audio.muted = !s.audio.muted; },
            [&](set_clear_color a) { _s.ui.colors.clear = a.color; },
            [&](set_audio_thread_running a) { _s.audio.running = a.running; },
            [&](set_action_consumer_running a) { _s.action_consumer.running = a.running; },
            [&](set_ui_running a) { _s.ui.running = a.running; },
            [&](close_application) {
                _s.ui.running = false;
                _s.audio.running = false;
                _s.action_consumer.running = false;
            }
        },
        action
    );
}
