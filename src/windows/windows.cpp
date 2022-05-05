#include "../context.h"

// TODO move `wrap_draw_in_window` into a new `StatefulImGuiWindowFlags : ImGuiWindowFlags` type
void draw_window(Window &window, ImGuiWindowFlags flags = ImGuiWindowFlags_None, bool wrap_draw_in_window = true) {
    const auto &name = window.name;
    if (s.windows.named(name).visible != window.visible) q(toggle_window{name});
    if (!window.visible) return;

    if (wrap_draw_in_window) {
        if (!ImGui::Begin(name.c_str(), &window.visible, flags)) {
            ImGui::End();
            return;
        }
    } else {
        if (!window.visible) return;
    }

    window.draw();

    if (wrap_draw_in_window) ImGui::End();
}

void Windows::draw() {
    draw_window(controls);

    draw_window(state.memory_editor, ImGuiWindowFlags_NoScrollbar);
    draw_window(state.viewer, ImGuiWindowFlags_MenuBar);
    draw_window(state.path_update_frequency, ImGuiWindowFlags_None);

    draw_window(style_editor);
    draw_window(demos, ImGuiWindowFlags_MenuBar);
    draw_window(metrics);

    draw_window(faust.editor, ImGuiWindowFlags_MenuBar);
    draw_window(faust.log);
}
