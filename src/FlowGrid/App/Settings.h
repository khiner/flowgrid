#pragma once

#include "Core/Field/Float.h"
#include "Core/Window.h"

struct ApplicationSettings : Window {
    using Window::Window;

    Prop(Float, GestureDurationSec, 0.5, 0, 5); // Merge actions occurring in short succession into a single gesture

protected:
    void Render() const override;
};

extern const ApplicationSettings &application_settings;
