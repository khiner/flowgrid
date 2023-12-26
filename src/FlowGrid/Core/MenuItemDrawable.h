#pragma once

struct MenuItemDrawable {
    virtual ~MenuItemDrawable() = default;
    virtual void MenuItem() const = 0;
};
