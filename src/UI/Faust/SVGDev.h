#pragma once

#include "Device.h"

class SVGDev : public device {
public:
    SVGDev(const char *, double, double);
    ~SVGDev() override;
    void rect(double x, double y, double l, double h, const char *color, const char *link) override;
    void triangle(double x, double y, double l, double h, const char *color, const char *link, bool leftright) override;
    void rond(double x, double y, double rayon) override;
    void carre(double x, double y, double cote) override;
    void fleche(double x, double y, double rotation, int sens) override;
    void trait(double x1, double y1, double x2, double y2) override;
    void dasharray(double x1, double y1, double x2, double y2) override;
    void text(double x, double y, const char *name, const char *link) override;
    void label(double x, double y, const char *name) override;
    void markSens(double x, double y, int sens) override;
    void Error(const char *message, const char *reason, int nb_error, double x, double y, double largeur) override;
};
