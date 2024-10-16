#pragma once

#include <functional>

struct ImGuiSettings;

namespace FlowGrid {
struct Style;
}

namespace fg = FlowGrid;

struct UIContext {
    UIContext(std::function<void()> draw, const ImGuiSettings &, const fg::Style &);
    ~UIContext();

    // Main UI tick function
    // Returns `true` if the app should continue running.
    bool Tick() const;

    const std::function<void()> Draw;
    const ImGuiSettings &Settings;
    const fg::Style &Style;
};
