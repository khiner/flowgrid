#pragma once

#include "BoolAction.h"
#include "Core/Action/Actionable.h"
#include "Primitive.h"

struct Bool : Primitive<bool>, Actionable<Action::Primitive::Bool::Any>, MenuItemDrawable {
    using Primitive::Primitive;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; };

    bool CheckedDraw() const; // Unlike `Draw`, this returns `true` if the value was toggled during the draw.
    void MenuItem() const override;

    void Toggle_() {
        Set_(!Get());
        Refresh();
    }

    void IssueToggle() const;

    void Render(std::string_view label) const;

private:
    void Render() const override;
};
