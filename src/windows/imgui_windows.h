#pragma once

#include "drawable.h"

struct ImGuiWindows : Drawable {
    void draw(Window &) override;

    struct Metrics : public Drawable { void draw(Window &) override; };
    struct Demo : public Drawable { void draw(Window &) override; };
    struct StyleEditor : public Drawable { void draw(Window &) override; };

    Demo demo{};
    Metrics metrics{};
    StyleEditor style_editor{};
};
