#pragma once

#include "string"

#include "imgui.h"

using std::string;

enum { kLeftRight = 1, kRightLeft = -1 };

struct Line {
    ImVec2 start, end;

    Line(const ImVec2 &p1, const ImVec2 &p2) : start(p1), end(p2) {}
};

class Device {
public:
    virtual ~Device() = default;
    virtual void rect(const ImVec4 &rect, const string &color, const string &link) = 0;
    virtual void triangle(const ImVec2 &pos, const ImVec2 &size, const string &color, int orientation, const string &link) = 0;
    virtual void circle(const ImVec2 &pos, float radius) = 0;
    virtual void arrow(const ImVec2 &pos, float rotation, int orientation) = 0;
    virtual void line(const Line &line) = 0;
    virtual void dasharray(const Line &line) = 0;
    virtual void text(const ImVec2 &pos, const char *name, const string &link) = 0;
    virtual void label(const ImVec2 &pos, const char *name) = 0;
    virtual void dot(const ImVec2 &pos, int orientation) = 0;
};
