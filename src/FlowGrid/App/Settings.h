#pragma once

#include "Core/Stateful/WindowMember.h"

WindowMember(
    ApplicationSettings,
    Prop(Float, GestureDurationSec, 0.5, 0, 5); // Merge actions occurring in short succession into a single gesture
);

extern const ApplicationSettings &application_settings;
