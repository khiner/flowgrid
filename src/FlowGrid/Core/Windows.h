#pragma once

#include "Field/Vector.h"
#include "WindowsAction.h"

struct Windows : Component, Drawable, MenuItemDrawable, Actionable<Action::Windows::Any> {
    using Component::Component;

    void SetWindowComponents(const std::vector<std::reference_wrapper<const Component>> &);

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; }
    bool IsWindow(ID id) const;
    void MenuItem() const override;

    Prop(Vector<bool>, VisibleComponents);

    std::vector<ID> WindowComponentIds;

protected:
    void Render() const override;
};
