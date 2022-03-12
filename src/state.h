#pragma once

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
    Window demo;
};


enum AudioBackend {
    none, dummy, alsa, pulseaudio, jack, coreaudio, wasapi
};

struct Audio {
    AudioBackend backend = none;
    char *device_id = nullptr;
    char *stream_name = nullptr;
    double latency = 0.0;
    int sample_rate = 0;
    bool raw = false;
    bool running = true;
};

struct Sine {
    bool on = false;
    int frequency = 440;
    float amplitude = 0.5;
};

struct State {
    Colors colors;
    Windows windows;
    Audio audio;
    Sine sine;
};

#include <nlohmann/json.hpp>

// JSON serializers
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Audio, backend, latency, sample_rate, raw, running) // TODO string fields
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Color, r, g, b, a)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Colors, clear)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Window, show)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Windows, demo)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Sine, on, frequency, amplitude)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(State, colors, windows, audio, sine, audio);
