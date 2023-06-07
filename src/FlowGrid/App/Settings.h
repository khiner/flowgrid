#pragma once

#include "Core/Stateful/Field/Float.h"
#include "Core/Stateful/Window.h"

DefineWindow(
    ApplicationSettings,
    Prop(Float, GestureDurationSec, 0.5, 0, 5); // Merge actions occurring in short succession into a single gesture
);

extern const ApplicationSettings &application_settings;
