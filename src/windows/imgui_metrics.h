#pragma once

#include "drawable.h"

struct ImGuiMetrics : public Drawable {
    void draw(Window &) override;
};
