#pragma once

#include "Field/Vector.h"
#include "WindowsAction.h"

struct Windows : Component, Drawable, MenuItemDrawable {
    using Component::Component;

    void SetWindowComponents(const std::vector<std::reference_wrapper<const Component>> &) const;

    void Apply(const Action::Windows::Any &) const;
    bool CanApply(const Action::Windows::Any &) const { return true; }
    bool IsVisible(ID id) const { return VisibleComponents.Contains(id); }
    void MenuItem() const override;

    Prop(Vector<ID>, VisibleComponents); // todo IsComponentVisible, with `Component::Index` as argument

protected:
    void Render() const override;
};
