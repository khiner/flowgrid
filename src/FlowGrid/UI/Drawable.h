#pragma once

struct Drawable {
    virtual void Draw() const; // Wraps around the internal `Render` function.

protected:
    virtual void Render() const = 0;
};

struct MenuItemDrawable {
    virtual void MenuItem() const = 0;
};
