#pragma once

#include "../State.h"

UiContext create_ui();
void tick_ui();
void destroy_ui();

/**
 * These are wrappers around ImGui widgets that make state handling easier.
 */

void dock_window(const Window &window, ImGuiID node_id);
void gestured();
