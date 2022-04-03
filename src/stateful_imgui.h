#pragma once

#include "state.h"

void dock_window(const Window &, ImGuiID node_id);
void window_toggle(const Window &);
void gestured();

namespace StatefulImGui {

bool ColorEdit3(const char *label, Color &color, ImGuiColorEditFlags flags = 0);

}
