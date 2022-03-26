#include <iostream>
#include "context.h"
#include "transformers/bijective/state2json.h"
#include "transformers/bijective/json2state.h"
#include "visitor.h"

Context::Context() : json_state(state2json(_state)) {}

void Context::on_action(const Action &action) {
    if (std::holds_alternative<undo>(action)) {
        if (can_undo()) apply_diff(actions[current_action_index--].reverse_diff);
    } else if (std::holds_alternative<redo>(action)) {
        if (can_redo()) apply_diff(actions[++current_action_index].forward_diff);
    } else {
        update(action);
        if (!in_gesture) finalize_gesture();
        audio_context.on_action(action); // audio-related side effects
    }
}

/**
 * Inspired by [`lager`](https://sinusoid.es/lager/architecture.html#reducer),
 * but only the action-visitor pattern remains.
 *
 * When updates need to happen atomically across linked members for logical consistency,
 * make working copies as needed. Otherwise, modify the (single, global) state directly, in-place.
 */
void Context::update(const Action &action) {
    State &_s = _state; // Convenient shorthand for the mutable state that doesn't conflict with the global `s` instance
    std::visit(
        visitor{
            // TODO _s.ui.windows[a.name].show
            [&](toggle_demo_window) { _s.ui.windows.demo.visible = !s.ui.windows.demo.visible; },
            [&](toggle_faust_editor_window) { _s.ui.windows.faust_editor.visible = !s.ui.windows.faust_editor.visible; },
            [&](toggle_faust_editor_open) { _s.ui.windows.faust_editor.open = !s.ui.windows.faust_editor.open; },

            [&](toggle_audio_muted) { _s.audio.muted = !s.audio.muted; },
            [&](set_clear_color a) { _s.ui.colors.clear = a.color; },
            [&](set_audio_thread_running a) { _s.audio.running = a.running; },
            [&](toggle_audio_running) { _s.audio.running = !s.audio.running; },
            [&](set_action_consumer_running a) { _s.action_consumer.running = a.running; },
            [&](set_ui_running a) { _s.ui.running = a.running; },
            [&](const set_faust_text &a) { _s.audio.faust.code = a.text; },
            [&](const set_audio_sample_rate &a) { _s.audio.sample_rate = a.sample_rate; },
            [&](close_application) {
                _s.ui.running = false;
                _s.audio.running = false;
                _s.action_consumer.running = false;
            },
            // All actions that don't affect state:
            [&](undo) {},
            [&](redo) {},
        },
        action
    );
}

void Context::apply_diff(const json &diff) {
    json_state = json_state.patch(diff);
    _state = json2state(json_state);
    ui_s = _state; // Update the UI-copy of the state to reflect.
}

void Context::finalize_gesture() {
    auto old_json_state = json_state;
    json_state = state2json(s);
    auto diff = json::diff(old_json_state, json_state);
    if (!diff.empty()) {
        while (int(actions.size()) > current_action_index + 1) actions.pop_back();
        actions.emplace_back(ActionDiff{diff, json::diff(json_state, old_json_state)});
        current_action_index = int(actions.size()) - 1;
        std::cout << "Action #" << actions.size() <<
                  ":\nforward_diff: " << actions.back().forward_diff <<
                  "\nreverse_diff: " << actions.back().reverse_diff << std::endl;
    }
}
