#pragma once

#include "Core/Component.h"

struct ComponentContainer : Component {
    ComponentContainer(ComponentArgs &&args, Menu &&menu) : Component(std::move(args), std::move(menu)) {
        FieldIds.insert(Id);
        ContainerIds.insert(Id);
        Refresh();
    }

    ComponentContainer(ComponentArgs &&args) : ComponentContainer(std::move(args), Menu{{}}) {}

    virtual ~ComponentContainer() {
        Erase();
        ContainerIds.erase(Id);
        FieldIds.erase(Id);
    }
};
