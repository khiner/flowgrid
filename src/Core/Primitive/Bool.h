#pragma once

#include "Primitive.h"

struct Bool : Primitive<bool>, MenuItemDrawable {
    using Primitive::Primitive;

    bool CheckedDraw() const; // Unlike `Draw`, this returns `true` if the value was toggled during the draw.
    void MenuItem() const override;

    void Toggle_(TransientStore &);
    void IssueToggle() const;

    void Render(std::string_view label) const;

private:
    void Render() const override;
};
