#pragma once

#include "Core/Window.h"

struct Info : Window {
    using Window::Window;

protected:
    void Render() const override;
};