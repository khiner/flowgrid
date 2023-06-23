#pragma once

struct Drawable;
struct ImFont;

struct UIContext {
    UIContext();
    ~UIContext();

    // Main UI tick function
    // Returns `true` if the app should continue running.
    bool Tick(const Drawable &app);

    inline void SetAllUpdateFlags() { UpdateFlags = Flags_ImGuiSettings | Flags_ImGuiStyle | Flags_ImPlotStyle; }

    enum Flags_ {
        Flags_None = 0,
        Flags_ImGuiSettings = 1 << 0,
        Flags_ImGuiStyle = 1 << 1,
        Flags_ImPlotStyle = 1 << 2,
    };
    using Flags = int;

    struct Fonts {
        ImFont *Main{nullptr};
        ImFont *FixedWidth{nullptr};
    };

    Fonts Fonts{};
    Flags UpdateFlags = Flags_None;
};

extern UIContext Ui; // Created in `main.cpp`
