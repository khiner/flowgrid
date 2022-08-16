#pragma once

#include "string"

using std::string;

enum { kLeftRight = 1, kRightLeft = -1 };

class Device {
public:
    virtual ~Device() = default;
    virtual void rect(float x, float y, float l, float h, const string &color, const string &link) = 0;
    virtual void triangle(float x, float y, float l, float h, const string &color, int orientation, const string &link) = 0;
    virtual void circle(float x, float y, float radius) = 0;
    virtual void square(float x, float y, float dim) = 0;
    virtual void arrow(float x, float y, float rotation, int orientation) = 0;
    virtual void line(float x1, float y1, float x2, float y2) = 0;
    virtual void dasharray(float x1, float y1, float x2, float y2) = 0;
    virtual void text(float x, float y, const char *name, const string &link) = 0;
    virtual void label(float x, float y, const char *name) = 0;
    virtual void dot(float x, float y, int orientation) = 0;
    virtual void Error(const char *message, const char *reason, int nb_error, float x, float y, float width) = 0;
};
