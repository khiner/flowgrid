#pragma once

#include "Field/Vector.h"
#include "WindowsAction.h"

struct Windows : Component, Drawable, MenuItemDrawable {
    using Component::Component;

    void SetWindowComponents(const std::vector<std::reference_wrapper<const Component>> &);

    void Apply(const Action::Windows::Any &) const;
    bool CanApply(const Action::Windows::Any &) const { return true; }
    bool IsVisible(ID id) const { return VisibleComponents.Contains(id); }
    void MenuItem() const override;

    Prop(Vector<bool>, VisibleComponents);

    std::vector<ID> WindowComponentIds;

protected:
    void Render() const override;
};
