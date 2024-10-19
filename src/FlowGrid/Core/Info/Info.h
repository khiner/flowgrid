#pragma once

#include "Core/Component.h"

struct Info : Component {
    using Component::Component;

protected:
    void Render() const override;
};
