#pragma once

#include "Core/Field/Field.h"

struct Bool : TypedField<bool>, MenuItemDrawable {
    using TypedField::TypedField;

    bool CheckedDraw() const; // Unlike `Draw`, this returns `true` if the value was toggled during the draw.
    void MenuItem() const override;

private:
    void Render() const override;
    void Toggle() const; // Used in draw methods.
};
