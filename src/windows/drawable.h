#pragma once

struct Drawable {
    virtual void draw() = 0;
    virtual void destroy() = 0;
};
