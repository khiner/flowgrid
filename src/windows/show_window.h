#pragma once

#include "drawable.h"

void draw_window(const std::string &name, Drawable &drawable, ImGuiWindowFlags flags = 0, bool wrap_draw_in_window = true);
