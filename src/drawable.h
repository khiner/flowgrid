#pragma once

struct Drawable {
    virtual void show() = 0;
    virtual void destroy() = 0;
};
