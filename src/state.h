#pragma once

#include <nlohmann/json.hpp>

struct Color {
    float r, g, b, a;

    Color() { r = g = b = a = 0.0f; }
    Color(float r, float g, float b) : r(r), g(g), b(b), a(0) {}
    Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}

    Color &operator+=(float x) &{
        r += x;
        g += x;
        b += x;
        return *this;
    }

    Color &operator*=(float x) &{
        r *= x;
        g *= x;
        b *= x;
        return *this;
    }

    Color &operator+=(Color const &c) &{
        r += c.r;
        g += c.g;
        b += c.b;
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

enum AudioBackend {
    none, dummy, alsa, pulseaudio, jack, coreaudio, wasapi
};

struct AudioConfig {
    bool raw = false;
    AudioBackend backend = none;
    char *device_id = nullptr;
    char *stream_name = nullptr;
    double latency = 0.0;
    int sample_rate = 0;
};

struct Sine {
    bool on = false;
    int frequency = 440;
    float amplitude = 0.5;
};

struct State {
    AudioConfig audio_config;
    Colors colors{};
    Sine sine{};
    bool show_demo_window = true;
    bool audio_engine_running = true;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AudioConfig, raw, backend, latency, sample_rate) // TODO string fields
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Color, r, g, b, a)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Colors, clear)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Sine, on, frequency, amplitude)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(State, audio_config, colors, show_demo_window, audio_engine_running, sine);
