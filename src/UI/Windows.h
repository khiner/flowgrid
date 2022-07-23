#pragma once

#include <string>

using std::string;

struct Drawable {
    virtual void draw() const = 0;
};

struct WindowData {
    explicit WindowData(string name) : name(std::move(name)) {}
    string name;
    bool visible{true};
};

struct Window : WindowData, Drawable {
    explicit Window(const string &name) : WindowData(name) {}
};

struct Windows : Drawable {
    void draw() const override;

    struct StateViewer : Window {
        using Window::Window;
        void draw() const override;

        enum LabelMode { annotated, raw };
        LabelMode label_mode{annotated};
        bool auto_select{true};
    };

    struct StateMemoryEditor : Window {
        using Window::Window;
        void draw() const override;
    };

    struct StatePathUpdateFrequency : Window {
        using Window::Window;
        void draw() const override;
    };

    struct Demo : Window {
        using Window::Window;
        void draw() const override;
    };

    struct Metrics : Window {
        using Window::Window;
        void draw() const override;

        bool show_relative_paths = true;
    };

    struct Tools : Window {
        using Window::Window;
        void draw() const override;
    };

    Demo demo{"Demo"};
    Metrics metrics{"Metrics"};
    Tools tools{"Tools"};
    StateViewer state_viewer{"State viewer"};
    StateMemoryEditor memory_editor{"State memory editor"};
    StatePathUpdateFrequency path_update_frequency{"State path update frequency"};
};
