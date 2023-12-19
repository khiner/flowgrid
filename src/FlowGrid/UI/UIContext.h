#pragma once

struct Component;
struct ImGuiSettings;

namespace FlowGrid {
struct Style;
}

namespace fg = FlowGrid;

struct UIContext {
    UIContext(const ImGuiSettings &, const fg::Style &);
    ~UIContext();

    // Main UI tick function
    // Returns `true` if the app should continue running.
    bool Tick(const Component &) const;

    const ImGuiSettings &Settings;
    const fg::Style &Style;
};
