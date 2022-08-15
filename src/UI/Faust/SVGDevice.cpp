#include "SVGDevice.h"

#include <sstream>
#include "fmt/core.h"
#include "../../Helper/File.h"

using namespace std;
using namespace fmt;

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

SVGDevice::SVGDevice(string file_name, double width, double height) : file_name(std::move(file_name)) {
    static const double scale = 0.5;

    stream << "<?xml version=\"1.0\"?>\n";
    stream << format(R"(<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" version="1.1" viewBox="0 0 {} {}")", width, height);
    stream << (scaledSVG ? " width=\"100%%\" height=\"100%%\">\n" : format(" width=\"{}mm\" height=\"{}mm\">\n", width * scale, height * scale));

    if (shadowBlur) {
        stream << "<defs>\n"
                  "   <filter id=\"filter\" filterRes=\"18\" x=\"0\" y=\"0\">\n"
                  "     <feGaussianBlur in=\"SourceGraphic\" stdDeviation=\"1.55\" result=\"blur\"/>\n"
                  "     <feOffset in=\"blur\" dx=\"3\" dy=\"3\"/>\n"
                  "   </filter>\n"
                  "</defs>\n";
    }
}

SVGDevice::~SVGDevice() {
    stream << "</svg>\n";
    FileIO::write(file_name, stream.str());
}

void SVGDevice::rect(double x, double y, double l, double h, const char *color, const char *link) {
    char buf[512];
    if (link != nullptr && link[0] != 0) stream << format("<a xlink:href=\"{}\">\n", xmlcode(link, buf)); // open the optional link tag

    // Shadow
    stream << format(R"(<rect x="{}" y="{}" width="{}" height="{}" )", x + 1, y + 1, l, h);
    stream << (shadowBlur ? "rx=\"0.1\" ry=\"0.1\" style=\"stroke:none;fill:#aaaaaa;;filter:url(#filter);\"/>\n"
                          : "rx=\"0\" ry=\"0\" style=\"stroke:none;fill:#cccccc;\"/>\n");

    // Rectangle
    stream << format("<rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\" rx=\"0\" ry=\"0\" style=\"stroke:none;fill:{};\"/>\n", x, y, l, h, color);
    if (link != nullptr && link[0] != 0) stream << "</a>\n"; // close the optional link tag
}

void SVGDevice::triangle(double x, double y, double l, double h, const char *color, const char *link, int orientation) {
    char buf[512];
    if (link != nullptr && link[0] != 0) stream << format("<a xlink:href=\"{}\">\n", xmlcode(link, buf)); // open the optional link tag

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
    stream << format("<polygon fill=\"{}\" stroke=\"black\" stroke-width=\".25\" points=\"{},{} {},{} {},{}\"/>\n", color, x0, y, x1, y + h / 2.0, x0, y + h);
    stream << format("<circle  fill=\"{}\" stroke=\"black\" stroke-width=\".25\" cx=\"{}\" cy=\"{}\" r=\"{}\"/>\n", color, x2, y + h / 2.0, radius);
}

void SVGDevice::circle(double x, double y, double radius) {
    stream << format("<circle cx=\"{}\" cy=\"{}\" r=\"{}\"/>\n", x, y, radius);
}

string line(double x1, double y1, double x2, double y2, double rotation, double x, double y) {
    return format("<line x1=\"{}\" y1=\"{}\" x2=\"{}\" y2=\"{}\" transform=\"rotate({},{},{})\" style=\"stroke: black; stroke-width:0.25;\"/>\n", x1, y1, x2, y2, rotation, x, y);
}

void SVGDevice::arrow(double x, double y, double rotation, int orientation) {
    const double dx = 3;
    const double dy = 1;
    const auto x1 = orientation == kLeftRight ? x - dx : x + dx;
    stream << line(x1, y - dy, x, y, rotation, x, y);
    stream << line(x1, y + dy, x, y, rotation, x, y);
}

void SVGDevice::square(double x, double y, double dim) {
    stream << format("<rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\" style=\"stroke: black;stroke-width:0.5;fill:none;\"/>\n", x - 0.5 * dim, y - dim, dim, dim);
}

void SVGDevice::trait(double x1, double y1, double x2, double y2) {
    stream << format("<line x1=\"{}\" y1=\"{}\" x2=\"{}\" y2=\"{}\"  style=\"stroke:black; stroke-linecap:round; stroke-width:0.25;\"/>\n", x1, y1, x2, y2);
}

void SVGDevice::dasharray(double x1, double y1, double x2, double y2) {
    stream << format("<line x1=\"{}\" y1=\"{}\" x2=\"{}\" y2=\"{}\"  style=\"stroke: black; stroke-linecap:round; stroke-width:0.25; stroke-dasharray:3,3;\"/>\n", x1, y1, x2, y2);
}

void SVGDevice::text(double x, double y, const char *name, const char *link) {
    char buf[512];
    if (link != nullptr && link[0] != 0) stream << format("<a xlink:href=\"{}\">\n", xmlcode(link, buf)); // open the optional link tag
    char name2[256];
    stream << format("<text x=\"{}\" y=\"{}\" font-family=\"Arial\" font-size=\"7\" text-anchor=\"middle\" fill=\"#FFFFFF\">{}</text>\n", x, y + 2, xmlcode(name, name2));
    if (link != nullptr && link[0] != 0) stream << "</a>\n"; // close the optional link tag
}

void SVGDevice::label(double x, double y, const char *name) {
    char name2[256];
    stream << format("<text x=\"{}\" y=\"{}\" font-family=\"Arial\" font-size=\"7\">{}</text>\n", x, y + 2, xmlcode(name, name2));
}

void SVGDevice::dot(double x, double y, int orientation) {
    const int offset = orientation == kLeftRight ? 2 : -2;
    stream << format("<circle cx=\"{}\" cy=\"{}\" r=\"1\"/>\n", x + offset, y + offset);
}

string errorText(double x, double y, double length, const string &stroke, const string &fill, const string &text) {
    return format(R"(<text x="{}" y="{}" textLength="{}" lengthAdjust="spacingAndGlyphs" style="stroke: {}; stroke-width:0.3; text-anchor:middle; fill:{};">{}</text>)", x, y, length, stroke, fill, text);
}

void SVGDevice::Error(const char *message, const char *reason, int nb_error, double x, double y, double width) {
    stream << errorText(x, y - 7, width, "red", "red", format("{} : {}", nb_error, message)) << '\n';
    stream << errorText(x, y + 7, width, "red", "none", reason) << '\n';
}
