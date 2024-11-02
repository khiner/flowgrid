#pragma once

#include <functional>

namespace flowgrid {
struct Style;
}

struct UIContext {
    UIContext(std::function<void()> predraw, std::function<void()> draw);
    ~UIContext();

    // Main UI tick function
    // Returns `true` if the app should continue running.
    bool Tick() const;

    const std::function<void()> Predraw, Draw;
};
