#include "SVGDevice.h"

#include <sstream>

using namespace std;

bool scaledSVG = false; // Draw scaled SVG files
bool shadowBlur = false; // Note: `svg2pdf` doesn't like the blur filter

static char *xmlcode(const char *name, char *name2) {
    int i, j;

    // Substitute characters prohibited in XML:
    for (i = 0, j = 0; name[i] != 0 && j < 250; i++) {
        switch (name[i]) {
            case '<':name2[j++] = '&';
                name2[j++] = 'l';
                name2[j++] = 't';
                name2[j++] = ';';
                break;
            case '>':name2[j++] = '&';
                name2[j++] = 'g';
                name2[j++] = 't';
                name2[j++] = ';';
                break;
            case '\'':name2[j++] = '&';
                name2[j++] = 'a';
                name2[j++] = 'p';
                name2[j++] = 'o';
                name2[j++] = 's';
                name2[j++] = ';';
                break;
            case '"':name2[j++] = '&';
                name2[j++] = 'q';
                name2[j++] = 'u';
                name2[j++] = 'o';
                name2[j++] = 't';
                name2[j++] = ';';
                break;
            case '&':name2[j++] = '&';
                name2[j++] = 'a';
                name2[j++] = 'm';
                name2[j++] = 'p';
                name2[j++] = ';';
                break;
            default:name2[j++] = name[i];
        }
    }
    name2[j] = 0;

    return name2;
}

SVGDevice::SVGDevice(const char *ficName, double width, double height) {
    static const double scale = 0.5;
    if ((fic_repr = fopen(ficName, "w+")) == nullptr) throw std::runtime_error(string("ERROR : impossible to create or open ") + ficName);

    // Representation file:
    fprintf(fic_repr, "<?xml version=\"1.0\"?>\n");

    // View box:
    const string shared = R"(<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" version="1.1" viewBox="0 0 %f %f")";
    scaledSVG ? fprintf(fic_repr, (shared + " width=\"100%%\" height=\"100%%\">\n").c_str(), width, height)
              : fprintf(fic_repr, (shared + " width=\"%fmm\" height=\"%fmm\">\n").c_str(), width, height, width * scale, height * scale);

    if (shadowBlur) {
        fprintf(fic_repr,
            "<defs>\n"
            "   <filter id=\"filter\" filterRes=\"18\" x=\"0\" y=\"0\">\n"
            "     <feGaussianBlur in=\"SourceGraphic\" stdDeviation=\"1.55\" result=\"blur\"/>\n"
            "     <feOffset in=\"blur\" dx=\"3\" dy=\"3\"/>\n"
            "   </filter>\n"
            "</defs>\n");
    }
}

SVGDevice::~SVGDevice() {
    fprintf(fic_repr, "</svg>\n");
    fclose(fic_repr);
}

void SVGDevice::rect(double x, double y, double l, double h, const char *color, const char *link) {
    char buf[512];
    if (link != nullptr && link[0] != 0) fprintf(fic_repr, "<a xlink:href=\"%s\">\n", xmlcode(link, buf)); // open the optional link tag

    // Shadow
    fprintf(fic_repr, shadowBlur ? "<rect x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" rx=\"0.1\" ry=\"0.1\" style=\"stroke:none;fill:#aaaaaa;;filter:url(#filter);\"/>\n"
                                 : "<rect x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" rx=\"0\" ry=\"0\" style=\"stroke:none;fill:#cccccc;\"/>\n", x + 1, y + 1, l, h);

    // Rectangle
    fprintf(fic_repr, "<rect x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" rx=\"0\" ry=\"0\" style=\"stroke:none;fill:%s;\"/>\n", x, y, l, h, color);
    if (link != nullptr && link[0] != 0) fprintf(fic_repr, "</a>\n"); // close the optional link tag
}

void SVGDevice::triangle(double x, double y, double l, double h, const char *color, const char *link, int orientation) {
    char buf[512];
    if (link != nullptr && link[0] != 0) fprintf(fic_repr, "<a xlink:href=\"%s\">\n", xmlcode(link, buf)); // open the optional link tag

    static const double radius = 1.5;
    double x0, x1, x2;
    if (orientation == kLeftRight) {
        x0 = x;
        x1 = x + l - 2 * radius;
        x2 = x + l - radius;
    } else {
        x0 = x + l;
        x1 = x + 2 * radius;
        x2 = x + radius;
    }
    // triangle + circle
    fprintf(fic_repr, "<polygon fill=\"%s\" stroke=\"black\" stroke-width=\".25\" points=\"%f,%f %f,%f %f,%f\"/>\n", color, x0, y, x1, y + h / 2.0, x0, y + h);
    fprintf(fic_repr, "<circle  fill=\"%s\" stroke=\"black\" stroke-width=\".25\" cx=\"%f\" cy=\"%f\" r=\"%f\"/>\n", color, x2, y + h / 2.0, radius);
}

void SVGDevice::circle(double x, double y, double radius) {
    fprintf(fic_repr, "<circle cx=\"%f\" cy=\"%f\" r=\"%f\"/>\n", x, y, radius);
}

void SVGDevice::arrow(double x, double y, double rotation, int orientation) {
    const double dx = 3;
    const double dy = 1;
    const auto x1 = orientation == kLeftRight ? x - dx : x + dx;
    const char *fmt = "<line x1=\"%f\" y1=\"%f\" x2=\"%f\" y2=\"%f\" transform=\"rotate(%f,%f,%f)\" style=\"stroke: black; stroke-width:0.25;\"/>\n";
    fprintf(fic_repr, fmt, x1, y - dy, x, y, rotation, x, y);
    fprintf(fic_repr, fmt, x1, y + dy, x, y, rotation, x, y);
}

void SVGDevice::square(double x, double y, double dim) {
    fprintf(fic_repr, "<rect x=\"%f\" y=\"%f\" width=\"%f\" height=\"%f\" style=\"stroke: black;stroke-width:0.5;fill:none;\"/>\n", x - 0.5 * dim, y - dim, dim, dim);
}

void SVGDevice::trait(double x1, double y1, double x2, double y2) {
    fprintf(fic_repr, "<line x1=\"%f\" y1=\"%f\" x2=\"%f\" y2=\"%f\"  style=\"stroke:black; stroke-linecap:round; stroke-width:0.25;\"/>\n", x1, y1, x2, y2);
}

void SVGDevice::dasharray(double x1, double y1, double x2, double y2) {
    fprintf(fic_repr, "<line x1=\"%f\" y1=\"%f\" x2=\"%f\" y2=\"%f\"  style=\"stroke: black; stroke-linecap:round; stroke-width:0.25; stroke-dasharray:3,3;\"/>\n", x1, y1, x2, y2);
}

void SVGDevice::text(double x, double y, const char *name, const char *link) {
    char buf[512];
    if (link != nullptr && link[0] != 0) fprintf(fic_repr, "<a xlink:href=\"%s\">\n", xmlcode(link, buf)); // open the optional link tag
    char name2[256];
    fprintf(fic_repr, "<text x=\"%f\" y=\"%f\" font-family=\"Arial\" font-size=\"7\" text-anchor=\"middle\" fill=\"#FFFFFF\">%s</text>\n", x, y + 2, xmlcode(name, name2));
    if (link != nullptr && link[0] != 0) fprintf(fic_repr, "</a>\n"); // close the optional link tag
}

void SVGDevice::label(double x, double y, const char *name) {
    char name2[256];
    fprintf(fic_repr, "<text x=\"%f\" y=\"%f\" font-family=\"Arial\" font-size=\"7\">%s</text>\n", x, y + 2, xmlcode(name, name2));
}

void SVGDevice::dot(double x, double y, int orientation) {
    const int offset = orientation == kLeftRight ? 2 : -2;
    fprintf(fic_repr, "<circle cx=\"%f\" cy=\"%f\" r=\"1\"/>\n", x + offset, y + offset);
}

void SVGDevice::Error(const char *message, const char *reason, int nb_error, double x, double y, double width) {
    const string shared = R"(<text x="%f" y="%f" textLength="%f" lengthAdjust="spacingAndGlyphs" style="stroke: red; stroke-width:0.3; text-anchor:middle;)";
    fprintf(fic_repr, (shared + " fill:red;\">%d : %s</text>\n").c_str(), x, y - 7, width, nb_error, message);
    fprintf(fic_repr, (shared + " fill:none;\">%s</text>\n").c_str(), x, y + 7, width, reason);
}
