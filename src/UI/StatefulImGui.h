#pragma once

#include "../State.h"
#include "../Action.h"

void dock_window(const Window &w, ImGuiID node_id);
void gestured();

namespace StatefulImGui {

void DrawWindow(const Window &window, ImGuiWindowFlags flags = ImGuiWindowFlags_None);
bool WindowToggleMenuItem(const Window &);

bool Checkbox(const char *label, bool v);
bool SliderFloat(const char *label, float *v, float v_min, float v_max, const char *format = "%.3f", ImGuiSliderFlags flags = 0);
bool SliderFloat2(const char *label, float v[2], float v_min, float v_max, const char *format = "%.3f", ImGuiSliderFlags flags = 0);
bool SliderInt(const char *label, int *v, int v_min, int v_max, const char *format = "%d", ImGuiSliderFlags flags = 0);
// Example: `StatefulImGui::SliderScalar("slider u64 full", ImGuiDataType_U64, &u64_v, &u64_min, &u64_max, "%" IM_PRIu64 " ns");`
bool SliderScalar(const char *label, ImGuiDataType data_type, void *p_data, const void *p_min, const void *p_max, const char *format = nullptr, ImGuiSliderFlags flags = 0);

bool DragFloat(const char *label, float *v, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char *format = "%.3f", ImGuiSliderFlags flags = 0);

bool ColorEdit4(const char *label, float col[4], ImGuiColorEditFlags flags = 0);

// For actions with no data members.
void MenuItem(ActionID);

}
