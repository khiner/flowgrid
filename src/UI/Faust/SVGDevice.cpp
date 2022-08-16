#include "SVGDevice.h"

#include <sstream>
#include <map>
#include "fmt/core.h"
#include "../../Helper/File.h"
#include "../../Helper/String.h"

using namespace std;
using namespace fmt;

bool scaledSVG = false; // Draw scaled SVG files
bool shadowBlur = false; // Note: `svg2pdf` doesn't like the blur filter

static string xml_sanitize(const string &name) {
    static std::map<char, string> replacements{{'<', "&lt;"}, {'>', "&gt;"}, {'\'', "&apos;"}, {'"', "&quot;"}, {'&', "&amp;"}};

    auto replaced_name = name;
    for (const auto &[c, replacement]: replacements) {
        replaced_name = replace(replaced_name, c, replacement);
    }
    return replaced_name;
}

SVGDevice::SVGDevice(string file_name, float width, float height) : file_name(std::move(file_name)) {
    static const float scale = 0.5;

    stream << format(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {} {}")", width, height);
    stream << (scaledSVG ? R"( width="100%" height="100%">)" : format(R"( width="{}mm" height="{}mm">)", width * scale, height * scale));

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

void SVGDevice::rect(float x, float y, float l, float h, const string &color, const string &link) {
    if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link)); // open the optional link tag

    // Shadow
    stream << format(R"(<rect x="{}" y="{}" width="{}" height="{}" )", x + 1, y + 1, l, h);
    stream << (shadowBlur ? R"(rx="0.1" ry="0.1" style="stroke:none;fill:#aaaaaa;;filter:url(#filter);"/>)"
                          : R"(rx="0" ry="0" style="stroke:none;fill:#cccccc;"/>)");

    // Rectangle
    stream << format(R"(<rect x="{}" y="{}" width="{}" height="{}" rx="0" ry="0" style="stroke:none;fill:{};"/>)", x, y, l, h, color);
    if (!link.empty()) stream << "</a>"; // close the optional link tag
}

void SVGDevice::triangle(float x, float y, float l, float h, const string &color, int orientation, const string &link) {
    if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link)); // open the optional link tag

    static const float radius = 1.5;
    float x0, x1, x2;
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
    stream << format(R"(<polygon fill="{}" stroke="black" stroke-width=".25" points="{},{} {},{} {},{}"/>)", color, x0, y, x1, y + h / 2.0, x0, y + h);
    stream << format(R"(<circle  fill="{}" stroke="black" stroke-width=".25" cx="{}" cy="{}" r="{}"/>)", color, x2, y + h / 2.0, radius);
}

void SVGDevice::circle(float x, float y, float radius) {
    stream << format(R"(<circle cx="{}" cy="{}" r="{}"/>)", x, y, radius);
}

string transform_line(float x1, float y1, float x2, float y2, float rotation, float x, float y) {
    return format(R"lit(<line x1="{}" y1="{}" x2="{}" y2="{}" transform="rotate({},{},{})" style="stroke: black; stroke-width:0.25;"/>)lit", x1, y1, x2, y2, rotation, x, y);
}

void SVGDevice::arrow(float x, float y, float rotation, int orientation) {
    const float dx = 3;
    const float dy = 1;
    const auto x1 = orientation == kLeftRight ? x - dx : x + dx;
    stream << transform_line(x1, y - dy, x, y, rotation, x, y);
    stream << transform_line(x1, y + dy, x, y, rotation, x, y);
}

void SVGDevice::square(float x, float y, float dim) {
    stream << format(R"(<rect x="{}" y="{}" width="{}" height="{}" style="stroke: black;stroke-width:0.5;fill:none;"/>)", x - 0.5 * dim, y - dim, dim, dim);
}

void SVGDevice::line(float x1, float y1, float x2, float y2) {
    stream << format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}"  style="stroke:black; stroke-linecap:round; stroke-width:0.25;"/>)", x1, y1, x2, y2);
}

void SVGDevice::dasharray(float x1, float y1, float x2, float y2) {
    stream << format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}"  style="stroke: black; stroke-linecap:round; stroke-width:0.25; stroke-dasharray:3,3;"/>)", x1, y1, x2, y2);
}

void SVGDevice::text(float x, float y, const char *name, const string &link) {
    if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link)); // open the optional link tag
    stream << format(R"(<text x="{}" y="{}" font-family="Arial" font-size="7" text-anchor="middle" fill="#FFFFFF">{}</text>)", x, y + 2, xml_sanitize(name));
    if (!link.empty()) stream << "</a>"; // close the optional link tag
}

void SVGDevice::label(float x, float y, const char *name) {
    stream << format(R"(<text x="{}" y="{}" font-family="Arial" font-size="7">{}</text>)", x, y + 2, xml_sanitize(name));
}

void SVGDevice::dot(float x, float y, int orientation) {
    const float offset = orientation == kLeftRight ? 2 : -2;
    stream << format(R"(<circle cx="{}" cy="{}" r="1"/>)", x + offset, y + offset);
}

string errorText(float x, float y, float length, const string &stroke, const string &fill, const string &text) {
    return format(R"(<text x="{}" y="{}" textLength="{}" lengthAdjust="spacingAndGlyphs" style="stroke: {}; stroke-width:0.3; text-anchor:middle; fill:{};">{}</text>)", x, y, length, stroke, fill, text);
}

void SVGDevice::Error(const char *message, const char *reason, int nb_error, float x, float y, float width) {
    stream << errorText(x, y - 7, width, "red", "red", format("{} : {}", nb_error, message));
    stream << errorText(x, y + 7, width, "red", "none", reason);
}
