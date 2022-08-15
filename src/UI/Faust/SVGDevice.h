#pragma once

#include "Device.h"
#include "string"
#include "sstream"

using std::string;

struct SVGDevice : Device {
    SVGDevice(string file_name, double width, double height);
    ~SVGDevice() override;
    void rect(double x, double y, double l, double h, const char *color, const char *link) override;
    void triangle(double x, double y, double l, double h, const char *color, const char *link, int orientation) override;
    void circle(double x, double y, double radius) override;
    void square(double x, double y, double dim) override;
    void arrow(double x, double y, double rotation, int orientation) override;
    void trait(double x1, double y1, double x2, double y2) override;
    void dasharray(double x1, double y1, double x2, double y2) override;
    void text(double x, double y, const char *name, const char *link) override;
    void label(double x, double y, const char *name) override;
    void dot(double x, double y, int orientation) override;
    void Error(const char *message, const char *reason, int nb_error, double x, double y, double width) override;

private:
    string file_name;
    std::stringstream stream;
};
