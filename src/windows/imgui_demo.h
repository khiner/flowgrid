#pragma once

#include "drawable.h"

struct ImGuiDemo : public Drawable {
    void draw(Window &) override;
};
