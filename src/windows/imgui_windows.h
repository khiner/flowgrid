#pragma once

#include "drawable.h"

namespace ImGuiWindows {

struct Metrics : public Drawable { void draw(Window &) override; };
struct Demo : public Drawable { void draw(Window &) override; };

};
