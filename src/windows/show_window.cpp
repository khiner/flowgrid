#include "show_window.h"
#include "../context.h"

void show_window(const std::string &name, Drawable &drawable) {
    const auto &w = s.ui.windows.at(name);
    auto &mutable_w = ui_s.ui.windows[name];
    if (mutable_w.visible != w.visible) q.enqueue(toggle_window{name});
    if (!w.visible) return;

    ImGui::SetNextWindowCollapsed(!w.open);
//    ImGui::SetNextWindowPos(w.dimensions.position, ImGuiCond_Appearing);
//    ImGui::SetNextWindowSize(w.dimensions.size, ImGuiCond_Appearing);

    bool open = ImGui::Begin(name.c_str(), &mutable_w.visible, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar);
    if (open != w.open) q.enqueue(toggle_window_open{name});
    if (!w.open) {
        ImGui::End();
        return;
    }

    drawable.show();
    ImGui::End();
}
