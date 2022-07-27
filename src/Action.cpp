#include "Action.h"

using namespace action;

std::variant<Action, bool> merge(const Action &a, const Action &b) {
    const ID a_id = get_id(a);
    const ID b_id = get_id(b);

    switch (a_id) {
        case id<toggle_window>:
        case id<toggle_state_viewer_auto_select>: return true;
        case id<open_empty_project>:
        case id<open_default_project>:
        case id<show_open_project_dialog>:
        case id<open_file_dialog>:
        case id<close_file_dialog>:
        case id<save_project>:
        case id<save_default_project>:
        case id<save_current_project>:
        case id<show_save_project_dialog>:
        case id<close_application>:
        case id<set_imgui_settings>:
        case id<set_imgui_color_style>:
        case id<set_implot_color_style>:
        case id<set_flowgrid_color_style>:
        case id<close_window>:
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
        case id<change_imgui_settings>:
        case id<undo>:
        case id<redo>:
        default: return false;
    }
}

Gesture action::compress_gesture_actions(const Gesture &actions) {
    Gesture compressed_actions;

    std::optional<const Action> active_action;
    for (size_t i = 0; i < actions.size(); i++) {
        if (!active_action.has_value()) active_action.emplace(actions[i]);
        const auto &a = active_action.value();
        const auto &b = actions[i + 1];
        const auto merged = merge(a, b);
        std::visit(visitor{
            // `true` result means the actions cancel out, so we add neither.
            // `false` result means they can't be merged, so we add both to the result.
            // `Action` result is a merged action.
            [&](const bool result) {
                if (result) {
                    i++;
                    active_action.reset();
                } else {
                    compressed_actions.emplace_back(active_action.value());
                    active_action.reset();
                }
            },
            [&](const Action &result) { active_action.emplace(result); },
        }, merged);
    }
    if (active_action.has_value()) compressed_actions.emplace_back(active_action.value());

    return compressed_actions;
}
