#pragma once

#include "nlohmann/json.hpp"
#include "imgui.h"

struct Color {
    float r = 0, g = 0, b = 0, a = 0;

    Color &operator+=(float x) &{
        r += x;
        g += x;
        b += x;
        return *this;
    }

    Color &operator+=(Color const &c) &{
        r += c.r;
        g += c.g;
        b += c.b;
        return *this;
    }

    Color &operator*=(float x) &{
        r *= x;
        g *= x;
        b *= x;
        return *this;
    }

    Color &operator*=(Color const &c) &{
        r *= c.r;
        g *= c.g;
        b *= c.b;
        return *this;
    }
};

// TODO Different modes, with different states (e.g. AudioTrackMode),
//  which control the default settings for
//    * Layout
//    * Node organization, move-rules
//    * Automatic connections-rules

struct Colors {
    Color clear{};
};

struct Dimensions {
    ImVec2 position;
    ImVec2 size;
};

struct Window {
    std::string name;
    bool visible{true};
};

struct Windows {
    Window controls{"Controls"};
    Window imgui_demo{"Dear ImGui Demo"};
    Window imgui_metrics{"Dear ImGui Metrics/Debugger"};
    Window faust_editor{"Faust"};
};

struct UI {
    bool running = true;
    Windows windows;
    std::map<std::string, Window> window_named{
        {windows.controls.name,      windows.controls},
        {windows.imgui_demo.name,    windows.imgui_demo},
        {windows.imgui_metrics.name, windows.imgui_metrics},
        {windows.faust_editor.name,  windows.faust_editor},
    };
    Colors colors;
};

enum AudioBackend {
    none, dummy, alsa, pulseaudio, jack, coreaudio, wasapi
};

struct Faust {
    bool simple_text_editor;
    std::string code{"import(\"stdfaust.lib\"); process = no.noise;\n"};
    std::string error{};
};

struct Audio {
    AudioBackend backend = none;
    Faust faust;
    char *in_device_id = nullptr;
    char *out_device_id = nullptr;
    bool running = true;
    bool muted = true;
    bool out_raw = false;
    int sample_rate = 48000;
    double latency = 0.0;

};

struct ActionConsumer {
    bool running = true;
};

struct State {
    UI ui;
    Audio audio;
    ActionConsumer action_consumer;
};

// External types
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ImVec2, x, y)

// Internal types
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Faust, simple_text_editor, code, error)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Audio, running, muted, backend, latency, sample_rate, out_raw, faust)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Color, r, g, b, a)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Colors, clear)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Dimensions, position, size)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Window, name, visible)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Windows, controls, imgui_demo, imgui_metrics, faust_editor)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UI, running, windows, colors, window_named)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ActionConsumer, running)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(State, ui, audio, action_consumer);
