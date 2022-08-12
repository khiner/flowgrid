#pragma once

#include <cstdio>

class device {
public:
    virtual ~device() = default;
    virtual void rect(double x, double y, double l, double h, const char *color, const char *link) = 0;
    virtual void triangle(double x, double y, double l, double h, const char *color, const char *link, bool leftright) = 0;
    virtual void rond(double x, double y, double rayon) = 0;
    virtual void carre(double x, double y, double cote) = 0;
    virtual void fleche(double x, double y, double rotation, int sens) = 0;
    virtual void trait(double x1, double y1, double x2, double y2) = 0;
    virtual void dasharray(double x1, double y1, double x2, double y2) = 0;
    virtual void text(double x, double y, const char *name, const char *link) = 0;
    virtual void label(double x, double y, const char *name) = 0;
    virtual void markSens(double x, double y, int sens) = 0;
    virtual void Error(const char *message, const char *reason, int nb_error, double x, double y, double largeur) = 0;

protected:
    FILE *fic_repr = nullptr;
};
