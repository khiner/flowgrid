#include "show_window.h"
#include "../context.h"

void show_window(const std::string &name, Drawable &drawable) {
    const auto &w = s.ui.windows.at(name);
    auto &mutable_w = ui_s.ui.windows[name];
    if (mutable_w.visible != w.visible) q.enqueue(toggle_window{name});
    if (!w.visible) return;

    if (!ImGui::Begin(name.c_str(), &mutable_w.visible)) {
        ImGui::End();
        return;
    }

    drawable.show();
    ImGui::End();
}
