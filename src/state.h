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

struct State {
    bool show_demo_window = true;
    Color clear_color {};
    bool audio_engine_running = true;
    bool sine_on = false;
    float sine_frequency = 440.0f;
    float sine_amplitude = 0.5f;
};
