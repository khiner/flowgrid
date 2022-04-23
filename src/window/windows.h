#pragma once

#include "drawable.h"

struct StyleEditor : Drawable {
    void draw(Window &) override;
};

struct Controls : public Drawable {
    void draw(Window &) override;
};

struct StateViewer : public Drawable {
    void draw(Window &) override;
};

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

void draw_window(const std::string &name, Drawable &drawable, ImGuiWindowFlags flags = 0, bool wrap_draw_in_window = true);
