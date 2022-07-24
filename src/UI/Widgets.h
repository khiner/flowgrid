#include "../State.h"

namespace FlowGrid {

// Helper to display a (?) mark which shows a tooltip when hovered. From `imgui_demo.cpp`.
void HelpMarker(const char *desc);

// Show a help marker with the provided `help` text in a hovered tooltip marker before the menu item
bool BeginMenuWithHelp(const char *label, const char *help, bool enabled = true);
bool MenuItemWithHelp(const char *label, const char *help, const char *shortcut = nullptr, bool selected = false, bool enabled = true);

void DockWindow(const Window &window, ImGuiID node_id);
void DrawWindow(const Window &window, ImGuiWindowFlags flags = ImGuiWindowFlags_None);
bool WindowToggleMenuItem(const Window &);

bool Checkbox(const char *path, const char *label = nullptr);
bool SliderFloat(const char *path, float v_min, float v_max, const char *format = "%.3f", ImGuiSliderFlags flags = 0, const char *label = nullptr);
bool SliderFloat2(const char *path, float v_min, float v_max, const char *format = "%.3f", ImGuiSliderFlags flags = 0);
bool SliderInt(const char *label, int *v, int v_min, int v_max, const char *format = "%d", ImGuiSliderFlags flags = 0);

bool DragFloat(const char *path, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char *format = "%.3f", ImGuiSliderFlags flags = 0, const char *label = nullptr);

bool ColorEdit4(const char *path, ImGuiColorEditFlags flags = 0, const char *label = nullptr);

using ActionID = size_t; // duplicate definition to avoid importing `Action.h`

// For actions with no data members.
void MenuItem(ActionID);

bool Combo(const char *path, const char *items_separated_by_zeros, int popup_max_height_in_items = -1);

}
