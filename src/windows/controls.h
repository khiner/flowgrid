#pragma once

#include <filesystem>
#include "drawable.h"

namespace fs = std::filesystem;

struct Controls : public Drawable {
    void draw(Window &) override;
};
