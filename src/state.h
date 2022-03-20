#pragma once

#include "nlohmann/json.hpp"

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

struct Window {
    bool show;
};

struct Windows {
    Window demo{};
};

struct UI {
    bool running = true;
    Windows windows;
    Colors colors;
};

enum AudioBackend {
    none, dummy, alsa, pulseaudio, jack, coreaudio, wasapi
};

struct Audio {
    AudioBackend backend = none;
    char *in_device_id = nullptr;
    char *out_device_id = nullptr;
    std::string faust_text{"import(\"stdfaust.lib\"); process = no.noise;\n"};
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

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Audio, running, muted, backend, latency, sample_rate, out_raw, faust_text)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Color, r, g, b, a)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Colors, clear)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Window, show)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Windows, demo)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UI, running, windows, colors)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ActionConsumer, running)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(State, ui, audio, action_consumer);
