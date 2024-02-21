#pragma once

struct ImFont;

struct Fonts {
    inline static const float AtlasScale = 2; // We rasterize to a scaled-up texture and scale down the font size globally, for sharper text.

    void Init(); // Not in constructor so we can instantiate but delay loading fonts until after ImGui is initialized.

    ImFont *Main{nullptr};
    ImFont *Monospace{nullptr};
};
