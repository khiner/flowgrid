#pragma once

#include <filesystem>
#include "zep.h"
#include "drawable.h"

namespace fs = std::filesystem;

struct Controls : public Drawable {
    void draw() override;
    void destroy() override {};
};
