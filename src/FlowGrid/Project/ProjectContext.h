#pragma once

#include "Core/Windows.h"
#include "Style/Style.h"

struct ProjectContext : Component {
    using Component::Component;

    Prop(fg::Style, Style);
    Prop(Windows, Windows);
};
