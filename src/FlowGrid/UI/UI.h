#pragma once

struct Component;
struct ImFont;
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
    bool Tick(const Component &);

    struct Fonts {
        ImFont *Main{nullptr};
        ImFont *FixedWidth{nullptr};
    };

    Fonts Fonts{};
    const ImGuiSettings &Settings;
    const fg::Style &Style;
};

extern UIContext Ui; // Created in `main.cpp`
