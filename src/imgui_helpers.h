#pragma once

// From `imgui_demo.cpp`
// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
void HelpMarker(const char *desc);

// Show a help marker with the provided `help` text in a hovered tooltip marker before the menu item
bool BeginMenuWithHelp(const char *label, const char *help, bool enabled = true);
bool MenuItemWithHelp(const char *label, const char *help, const char *shortcut = nullptr, bool selected = false, bool enabled = true);
