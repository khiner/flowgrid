#include "show_window.h"
#include "../context.h"

void draw_window(const std::string &name, Drawable &drawable, ImGuiWindowFlags flags, bool wrap_draw_in_window) {
    const auto &w = s.ui.windows.at(name);
    auto &mutable_w = ui_s.ui.windows[name];
    if (mutable_w.visible != w.visible) q.enqueue(toggle_window{name});
    if (!w.visible) return;

    if (wrap_draw_in_window) {
        if (!ImGui::Begin(name.c_str(), &mutable_w.visible, flags)) {
            ImGui::End();
            return;
        }
    } else {
        if (!mutable_w.visible) return;
    }

    drawable.draw(mutable_w);

    if (wrap_draw_in_window) ImGui::End();
}
