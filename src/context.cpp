#include <iostream>
#include "context.h"
#include "transformers/bijective/state2json.h"
#include "transformers/bijective/json2state.h"
#include "visitor.h"

std::ostream &operator<<(std::ostream &os, const ActionDiff &diff) {
    return (os << "\tJSON diff:\n" << diff.json_diff << "\n\tINI diff:\n" << diff.ini_diff);
}
std::ostream &operator<<(std::ostream &os, const ActionDiffs &diffs) {
    return (os << "Forward:\n" << diffs.forward << "\nReverse:\n" << diffs.reverse);
}

Context::Context() : json_state(state2json(_state)) {}

void Context::on_action(const Action &action) {
    if (std::holds_alternative<undo>(action)) {
        if (can_undo()) apply_diff(actions[current_action_index--].reverse);
    } else if (std::holds_alternative<redo>(action)) {
        if (can_redo()) apply_diff(actions[++current_action_index].forward);
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
            [&](const set_ini_settings &a) { c.ini_settings = a.settings; },
            [&](const toggle_window &a) { _s.ui.window_named[a.name].visible = !s.ui.window_named.at(a.name).visible; },

            [&](toggle_audio_muted) { _s.audio.muted = !s.audio.muted; },
            [&](set_clear_color a) { _s.ui.colors.clear = a.color; },
            [&](set_audio_thread_running a) { _s.audio.running = a.running; },
            [&](toggle_audio_running) { _s.audio.running = !s.audio.running; },
            [&](set_action_consumer_running a) { _s.action_consumer.running = a.running; },
            [&](set_ui_running a) { _s.ui.running = a.running; },
            [&](const set_audio_sample_rate &a) { _s.audio.sample_rate = a.sample_rate; },

            [&](const set_faust_text &a) { _s.audio.faust.code = a.text; },
            [&](toggle_faust_simple_text_editor) { _s.audio.faust.simple_text_editor = !s.audio.faust.simple_text_editor; },

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

void Context::apply_diff(const ActionDiff &diff) {
    const auto[new_ini_settings, successes] = dmp.patch_apply(dmp.patch_fromText(diff.ini_diff), ini_settings);
    if (!std::all_of(successes.begin(), successes.end(), [](bool v) { return v; })) {
        throw std::runtime_error("Some ini-settings patches were not successfully applied.\nSettings:\n\t" +
            ini_settings + "\nPatch:\n\t" + diff.ini_diff + "\nResult:\n\t" + new_ini_settings);
    }
    ini_settings = prev_ini_settings = new_ini_settings;
    json_state = json_state.patch(diff.json_diff);
    _state = json2state(json_state);
    ui_s = _state; // Update the UI-copy of the state to reflect.
    if (!diff.ini_diff.empty()) has_new_ini_settings = true;
}

void Context::finalize_gesture() {
    auto old_json_state = json_state;
    json_state = state2json(s);
    auto json_diff = json::diff(old_json_state, json_state);

    auto old_ini_settings = prev_ini_settings;
    prev_ini_settings = ini_settings;
    auto ini_settings_patches = dmp.patch_make(old_ini_settings, ini_settings);

    if (!json_diff.empty() || !ini_settings_patches.empty()) {
        while (int(actions.size()) > current_action_index + 1) actions.pop_back();

        // TODO put diff/patch/text fns in `transformers/bijective`
        auto ini_settings_diff = diff_match_patch<std::string>::patch_toText(ini_settings_patches);
        auto ini_settings_reverse_diff = diff_match_patch<std::string>::patch_toText(dmp.patch_make(ini_settings, old_ini_settings));
        actions.emplace_back(ActionDiffs{
            {json_diff,                              ini_settings_diff},
            {json::diff(json_state, old_json_state), ini_settings_reverse_diff},
        });
        current_action_index = int(actions.size()) - 1;
        std::cout << "Action #" << actions.size() << ":\nDiffs:\n" << actions.back() << std::endl;
    }
}
