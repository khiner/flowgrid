#include "../State.h"

namespace FlowGrid {

void DrawWindow(const Window &window, ImGuiWindowFlags flags = ImGuiWindowFlags_None);
bool WindowToggleMenuItem(const Window &);

bool Checkbox(const JsonPath &path, const char *label = nullptr);
bool SliderFloat(const JsonPath &path, float v_min, float v_max, const char *format = "%.3f", ImGuiSliderFlags flags = 0, const char *label = nullptr);
bool SliderFloat2(const JsonPath &path, float v_min, float v_max, const char *format = "%.3f", ImGuiSliderFlags flags = 0);
bool SliderInt(const char *label, int *v, int v_min, int v_max, const char *format = "%d", ImGuiSliderFlags flags = 0);

bool DragFloat(const JsonPath &path, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char *format = "%.3f", ImGuiSliderFlags flags = 0, const char *label = nullptr);

bool ColorEdit4(const JsonPath &path, ImGuiColorEditFlags flags = 0, const char *label = nullptr);

// For actions with no data members.
void MenuItem(ActionID);

bool Combo(const JsonPath &path, const char *items_separated_by_zeros, int popup_max_height_in_items = -1);

}
