#include "show_window.h"
#include "../context.h"

void draw_window(const std::string &name, Drawable &drawable, ImGuiWindowFlags flags, bool wrap_draw_in_window) {
    const auto &w = s.ui.window_named.at(name);
    auto &_w = ui_s.ui.window_named[name];
    if (w.visible != _w.visible) q.enqueue(toggle_window{_w.name});
    if (!_w.visible) return;

    if (wrap_draw_in_window) {
        if (!ImGui::Begin(w.name.c_str(), &_w.visible, flags)) {
            ImGui::End();
            return;
        }
    } else {
        if (!_w.visible) return;
    }

    drawable.draw(_w);

    if (wrap_draw_in_window) ImGui::End();
}
