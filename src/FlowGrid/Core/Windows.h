#pragma once

#include "Core/Container/PrimitiveVector.h"
#include "WindowsAction.h"

struct Windows : Component, Actionable<Action::Windows::Any> {
    using Component::Component;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; }

    void SetWindowComponents(const std::vector<std::reference_wrapper<const Component>> &);

    bool IsWindow(ID component_id) const;
    bool IsVisible(ID component_id) const;

    void ToggleMenuItem(const Component &) const;
    void ToggleDebugMenuItem(const Component &) const;

    Prop(PrimitiveVector<ID>, VisibleComponents);

    std::vector<ID> WindowComponentIds;

protected:
    void Render() const override;

private:
    void ToggleVisible(ID component_id) const;
};
