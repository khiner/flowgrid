#pragma once

#include "BoolAction.h"
#include "Core/Field/Field.h"

struct Bool : TypedField<bool>, Actionable<Action::Primitive::Bool::Any>, MenuItemDrawable {
    using TypedField::TypedField;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    bool CheckedDraw() const; // Unlike `Draw`, this returns `true` if the value was toggled during the draw.
    void MenuItem() const override;

private:
    void Render() const override;
    void Toggle() const; // Issue toggle action. Used in draw methods.
};
