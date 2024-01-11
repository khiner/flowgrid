#pragma once

#include "Core/Component.h"

struct Container : Component {
    Container(ComponentArgs &&args, Menu &&menu) : Component(std::move(args), std::move(menu)) {
        FieldIds.insert(Id);
        ContainerIds.insert(Id);
        Refresh();
    }

    Container(ComponentArgs &&args) : Container(std::move(args), Menu{{}}) {}

    virtual ~Container() {
        Erase();
        ContainerIds.erase(Id);
        FieldIds.erase(Id);
    }
};
