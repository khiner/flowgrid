#pragma once

#include "drawable.h"

struct StyleEditor : Drawable {
    void draw(Window &) override;
};

struct Controls : public Drawable {
    void draw(Window &) override;
};

namespace StateWindows {
struct StateViewer : public Drawable { void draw(Window &) override; };
struct StatePathUpdateFrequency : public Drawable { void draw(Window &) override; };
struct MemoryEditorWindow : public Drawable { void draw(Window &) override; }; // Don't clash with `MemoryEditor` struct included from `imgui_memory_editor.h`
}

struct FaustEditor : public Drawable {
    void draw(Window &) override;
    void destroy() override;
};

struct FaustLog : public Drawable {
    void draw(Window &) override;
};

namespace ImGuiWindows {
struct Metrics : public Drawable { void draw(Window &) override; };
struct Demo : public Drawable { void draw(Window &) override; };

struct ImPlotWindows {
    struct Demo : public Drawable { void draw(Window &) override; };
};
};

void draw_window(const std::string &name, Drawable &drawable, ImGuiWindowFlags flags = ImGuiWindowFlags_None, bool wrap_draw_in_window = true);
