#pragma once

#include "BoolAction.h"
#include "PrimitiveField.h"

struct Bool : PrimitiveField<bool>, Actionable<Action::Primitive::Bool::Any>, MenuItemDrawable {
    using PrimitiveField::PrimitiveField;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    bool CheckedDraw() const; // Unlike `Draw`, this returns `true` if the value was toggled during the draw.
    void MenuItem() const override;

    void IssueToggle() const;

    void Render(string_view label) const;

private:
    void Render() const override;
};
