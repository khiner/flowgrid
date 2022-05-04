#pragma once

#include "state.h"

void dock_window(const Window &w, ImGuiID node_id);
void gestured();

namespace StatefulImGui {

bool WindowToggleMenuItem(const Window &);

bool SliderFloat(const char *label, float *v, float v_min, float v_max, const char *format = "%.3f", ImGuiSliderFlags flags = 0);
bool SliderFloat2(const char *label, float v[2], float v_min, float v_max, const char *format = "%.3f", ImGuiSliderFlags flags = 0);

bool DragFloat(const char *label, float *v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char *format = "%.3f", ImGuiSliderFlags flags = 0);

bool ColorEdit4(const char *label, float col[4], ImGuiColorEditFlags flags = 0);

}
