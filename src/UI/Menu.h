#pragma once

// Helper to display a (?) mark which shows a tooltip when hovered. From `imgui_demo.cpp`.
void HelpMarker(const char *desc);

// Show a help marker with the provided `help` text in a hovered tooltip marker before the menu item
bool BeginMenuWithHelp(const char *label, const char *help, bool enabled = true);
bool MenuItemWithHelp(const char *label, const char *help, const char *shortcut = nullptr, bool selected = false, bool enabled = true);
