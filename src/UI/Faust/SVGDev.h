#pragma once

#include "Device.h"

class LIBFAUST_API SVGDev : public device {
public:
    SVGDev(const char *, double, double);
    ~SVGDev();
    void rect(double x, double y, double l, double h, const char *color, const char *link);
    void triangle(double x, double y, double l, double h, const char *color, const char *link, bool leftright);
    void rond(double x, double y, double rayon);
    void carre(double x, double y, double cote);
    void fleche(double x, double y, double rotation, int sens);
    void trait(double x1, double y1, double x2, double y2);
    void dasharray(double x1, double y1, double x2, double y2);
    void text(double x, double y, const char *name, const char *link);
    void label(double x, double y, const char *name);
    void markSens(double x, double y, int sens);
    void Error(const char *message, const char *reason, int nb_error, double x, double y, double largeur);
};
