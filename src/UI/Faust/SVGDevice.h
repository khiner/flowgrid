#pragma once

#include "Device.h"
#include "sstream"


struct SVGDevice : Device {
    SVGDevice(string file_name, double width, double height);
    ~SVGDevice() override;
    void rect(double x, double y, double l, double h, const string &color, const string &link) override;
    void triangle(double x, double y, double l, double h, const string &color, int orientation, const string &link) override;
    void circle(double x, double y, double radius) override;
    void square(double x, double y, double dim) override;
    void arrow(double x, double y, double rotation, int orientation) override;
    void line(double x1, double y1, double x2, double y2) override;
    void dasharray(double x1, double y1, double x2, double y2) override;
    void text(double x, double y, const char *name, const string &link) override;
    void label(double x, double y, const char *name) override;
    void dot(double x, double y, int orientation) override;
    void Error(const char *message, const char *reason, int nb_error, double x, double y, double width) override;

private:
    string file_name;
    std::stringstream stream;
};
