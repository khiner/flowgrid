#pragma once

#include "Core/Component/Window.h"
#include "Core/Field/Float.h"

DefineWindow(
    ApplicationSettings,
    Prop(Float, GestureDurationSec, 0.5, 0, 5); // Merge actions occurring in short succession into a single gesture
);

extern const ApplicationSettings &application_settings;
