#pragma once

#include "../state.h"

struct StyleEditor : Window { void draw() override; };

struct Controls : public Window { void draw() override; };

namespace StateWindows {
struct StateViewer : public Window { void draw() override; };
struct StatePathUpdateFrequency : public Window { void draw() override; };
struct MemoryEditorWindow : public Window { void draw() override; }; // Don't clash with `MemoryEditor` struct included from `imgui_memory_editor.h`
}

struct FaustEditor : public Window {
    void draw() override;
    void destroy() override;
};

struct FaustLog : public Window {
    void draw() override;
};

struct Metrics : public Window { void draw() override; };
struct Demos : public Window { void draw() override; };

void draw_window(const std::string &name, Window &window, ImGuiWindowFlags flags = ImGuiWindowFlags_None, bool wrap_draw_in_window = true);
