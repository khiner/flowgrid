#pragma once

#include "../state.h"

struct Drawable {
    virtual void draw(Window &) = 0;
    virtual void destroy() {};
};
