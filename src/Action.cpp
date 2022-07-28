#include "Action.h"

using namespace action;

/**
 Provided actions are assumed to be chronologically consecutive.

 Cases:
 * `b` can be merged into `a`: return the merged action
 * `b` cancels out `a` (e.g. two consecutive boolean toggles on the same value): return `true`
 * `b` cannot be merged into `a`: return `false`

 Only handling cases where merges can be determined from two consecutive actions.
 One could imagine cases where an idempotent cycle could be determined only from > 2 actions.
 For example, incrementing modulo N would require N consecutive increments to determine that they could all be cancelled out.
*/
std::variant<Action, bool> merge(const Action &a, const Action &b) {
    const ID a_id = get_id(a);
    const ID b_id = get_id(b);

    switch (a_id) {
        case id<undo>: return b_id == id<redo>;
        case id<redo>: return b_id == id<undo>;
        case id<toggle_state_viewer_auto_select>: return a_id == b_id;
        case id<open_empty_project>:
        case id<open_default_project>:
        case id<show_open_project_dialog>:
        case id<open_file_dialog>:
        case id<close_file_dialog>:
        case id<show_save_project_dialog>:
        case id<close_application>:
        case id<set_imgui_settings>:
        case id<set_imgui_color_style>:
        case id<set_implot_color_style>:
        case id<set_flowgrid_color_style>:
        case id<set_state_viewer_label_mode>:
        case id<set_audio_sample_rate>:
        case id<set_faust_code>:
        case id<show_open_faust_file_dialog>:
        case id<show_save_faust_file_dialog>:
        case id<set_ui_running>:
            if (a_id == b_id) return b;
            else return false;
        case id<open_project>:
        case id<open_faust_file>:
        case id<save_faust_file>:
            if (a_id == b_id && json(a) == json(b)) return a;
            else return false;
        case id<set_value>:
            if (a_id == b_id && std::get<set_value>(a).state_path == std::get<set_value>(b).state_path) return b;
            else return false;
        case id<change_imgui_settings>: // Can't combine diffs currently.
        default: return false;
    }
}

Gesture action::compress_gesture(const Gesture &gesture) {
    Gesture compressed_gesture;

    std::optional<const Action> active_action;
    for (size_t i = 0; i < gesture.size(); i++) {
        if (!active_action.has_value()) active_action.emplace(gesture[i]);
        const auto &a = active_action.value();
        const auto &b = gesture[i + 1];
        const auto merged = merge(a, b);
        std::visit(visitor{
            [&](const bool result) {
                if (result) i++; // The two actions in consideration (`a` and `b`) cancel out, so we add neither. (Skip over `b` entirely.)
                else compressed_gesture.emplace_back(a); // The left-side action (`a`) can't be merged into any further - nothing more we can do for it!
                active_action.reset(); // No merge in either case. Move on to try compressing the next action.
            },
            [&](const Action &result) {
                active_action.emplace(result); // `Action` result is a merged action. Don't add it yet - maybe we can merge more actions into it.
            },
        }, merged);
    }
    if (active_action.has_value()) compressed_gesture.emplace_back(active_action.value());

    return compressed_gesture;
}
