#pragma once

struct Drawable;
struct ImFont;

struct UIContext {
    UIContext();
    ~UIContext();

    // Main UI tick function
    // Returns `true` if the app should continue running.
    bool Tick(const Drawable &app);

    struct Fonts {
        ImFont *Main{nullptr};
        ImFont *FixedWidth{nullptr};
    };

    Fonts Fonts{};
};

extern UIContext Ui; // Created in `main.cpp`
