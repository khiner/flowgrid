#pragma once

struct Color {
    float r, g, b, a;

    Color() { r = g = b = a = 0.0f; }
    Color(float r, float g, float b) : r(r), g(g), b(b), a(0) {}
    Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}

    Color& operator+=(float x)& {
        r += x;
        g += x;
        b += x;
        return *this;
    }

    Color& operator*=(float x)& {
        r *= x;
        g *= x;
        b *= x;
        return *this;
    }

    Color& operator+=(Color const& c)& {
        r += c.r;
        g += c.g;
        b += c.b;
        return *this;
    }

    Color& operator*=(Color const& c)& {
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
    Color clear {};
};

struct State {
    Colors colors {};
    bool show_demo_window = true;
    bool audio_engine_running = true;
    bool sine_on = false;
    float sine_frequency = 440.0f;
    float sine_amplitude = 0.5f;
};
