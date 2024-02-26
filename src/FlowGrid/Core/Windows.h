#pragma once

#include "ActionableComponent.h"
#include "Core/Container/PrimitiveSet.h"
#include "WindowsAction.h"

struct Windows : ActionableComponent<Action::Windows::Any> {
    using ActionableComponent::ActionableComponent;

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override { return true; }

    void SetWindowComponents(const std::vector<std::reference_wrapper<const Component>> &);

    bool IsWindow(ID component_id) const;
    bool IsVisible(ID component_id) const;

    void ToggleMenuItem(const Component &) const;
    void ToggleDebugMenuItem(const Component &) const;

    Prop(PrimitiveSet<ID>, VisibleComponents);

protected:
    void Render() const override;

private:
    std::set<ID> WindowComponentIds;

    void ToggleVisible(ID component_id) const;
};
