#pragma once

#include <set>

#include "Core/ActionProducerComponent.h"
#include "Core/Container/Set.h"
#include "WindowsAction.h"

struct Windows : ActionProducerComponent<Action::Windows::Any> {
    using ActionProducerComponent::ActionProducerComponent;

    void SetWindowComponents(const std::vector<std::reference_wrapper<const Component>> &);

    bool IsWindow(ID component_id) const;
    bool IsVisible(ID component_id) const;

    void ToggleVisible(ID component_id) const;
    void ToggleDebugMenuItem(const Component &) const;

    Prop(Set<ID>, VisibleComponents);

protected:
    void Render() const override;

private:
    std::set<ID> WindowComponentIds;
};
