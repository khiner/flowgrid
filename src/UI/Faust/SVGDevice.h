#pragma once

#include "Device.h"
#include "sstream"

struct SVGDevice : Device {
    SVGDevice(string file_name, float width, float height);
    ~SVGDevice() override;
    void rect(const ImVec4 &rect, const string &color, const string &link) override;
    void triangle(const ImVec2 &pos, const ImVec2 &size, const string &color, int orientation, const string &link) override;
    void circle(const ImVec2 &pos, float radius) override;
    void arrow(const ImVec2 &pos, float rotation, int orientation) override;
    void line(const Line &line) override;
    void dasharray(const Line &line) override;
    void text(const ImVec2 &pos, const char *name, const string &link) override;
    void label(const ImVec2 &pos, const char *name) override;
    void dot(const ImVec2 &pos, int orientation) override;

private:
    string file_name;
    std::stringstream stream;
};
