#pragma once

#include "Device.h"
#include "sstream"

struct SVGDevice : Device {
    SVGDevice(string file_name, float width, float height);
    ~SVGDevice() override;
    void rect(float x, float y, float l, float h, const string &color, const string &link) override;
    void triangle(float x, float y, float l, float h, const string &color, int orientation, const string &link) override;
    void circle(float x, float y, float radius) override;
    void square(float x, float y, float dim) override;
    void arrow(float x, float y, float rotation, int orientation) override;
    void line(float x1, float y1, float x2, float y2) override;
    void dasharray(float x1, float y1, float x2, float y2) override;
    void text(float x, float y, const char *name, const string &link) override;
    void label(float x, float y, const char *name) override;
    void dot(float x, float y, int orientation) override;
    void Error(const char *message, const char *reason, int nb_error, float x, float y, float width) override;

private:
    string file_name;
    std::stringstream stream;
};
