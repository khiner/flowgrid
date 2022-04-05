#pragma once

#include "drawable.h"

struct FaustEditor : public Drawable {
    void draw(Window &) override;
    void destroy() override;
};
