#pragma once

#include <string>

using std::string;

struct Drawable {
    virtual void draw() const = 0;
};

struct WindowData {
    string name;
    bool visible{true};
};

struct Window : WindowData, Drawable {
    Window() = default;
};

struct Windows : Drawable {
    void draw() const override;

    struct StateViewer : Window {
        StateViewer() { name = "State viewer"; }
        void draw() const override;

        enum LabelMode { annotated, raw };
        LabelMode label_mode{annotated};
        bool auto_select{true};
    };

    struct StateMemoryEditor : Window {
        StateMemoryEditor() { name = "State memory editor"; }
        void draw() const override;
    };

    struct StatePathUpdateFrequency : Window {
        StatePathUpdateFrequency() { name = "State path update frequency"; }
        void draw() const override;
    };

    struct Demo : Window {
        Demo() { name = "Demo"; }
        void draw() const override;
    };

    struct Metrics : Window {
        Metrics() { name = "Metrics"; }
        void draw() const override;
    };

    struct Tools : Window {
        Tools() { name = "Tools"; }
        void draw() const override;
    };

    Demo demo{};
    Metrics metrics{};
    Tools tools{};
    StateViewer state_viewer{};
    StateMemoryEditor memory_editor{};
    StatePathUpdateFrequency path_update_frequency{};
};
