#pragma once

#include <set>

#include "ActionProducerComponent.h"
#include "Container/Set.h"
#include "WindowsAction.h"

struct Windows : ActionProducerComponent<Action::Windows::Any> {
    using ActionProducerComponent::ActionProducerComponent;

    void Register(ID, bool dock = true);

    bool IsDock(ID) const;

    bool IsWindow(ID) const;
    bool IsVisible(ID) const;
    void ToggleVisible(ID) const;

    void DrawMenuItem(const Component &) const;

    Prop(Set<ID>, VisibleComponents);

protected:
    void Render() const override;

private:
    std::set<ID> DockComponentIds;
    std::set<ID> WindowComponentIds;
};
