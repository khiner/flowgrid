#pragma once

enum { kLeftRight = 1, kRightLeft = -1 };

class Device {
public:
    virtual ~Device() = default;
    virtual void rect(double x, double y, double l, double h, const char *color, const char *link) = 0;
    virtual void triangle(double x, double y, double l, double h, const char *color, const char *link, int orientation) = 0;
    virtual void circle(double x, double y, double radius) = 0;
    virtual void square(double x, double y, double dim) = 0;
    virtual void arrow(double x, double y, double rotation, int orientation) = 0;
    virtual void line(double x1, double y1, double x2, double y2) = 0;
    virtual void dasharray(double x1, double y1, double x2, double y2) = 0;
    virtual void text(double x, double y, const char *name, const char *link) = 0;
    virtual void label(double x, double y, const char *name) = 0;
    virtual void dot(double x, double y, int orientation) = 0;
    virtual void Error(const char *message, const char *reason, int nb_error, double x, double y, double largeur) = 0;
};
