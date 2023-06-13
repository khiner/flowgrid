#pragma once

#include "Core/Component.h"

struct Info : Component, Drawable {
    using Component::Component;

protected:
    void Render() const override;
};