#include "DrawBox.hh"

#include <sstream>
#include <set>
#include <map>
#include <stack>
#include <filesystem>
#include <fmt/core.h>

#include "../../Helper/String.h"
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/take_while.hpp>

#include "property.hh"
#include "boxes/ppbox.hh"
#include "faust/dsp/libfaust-box.h"
#include "faust/dsp/libfaust-signal.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "../../Helper/File.h"
#include "../../Helper/assert.h"

using namespace fmt;

enum { kLeftRight = 1, kRightLeft = -1 };

struct Line {
    ImVec2 start, end;
    Line(const ImVec2 &p1, const ImVec2 &p2) : start(p1), end(p2) {}
};

class Device {
public:
    virtual ~Device() = default;
    virtual void rect(const ImVec4 &rect, const string &color, const string &link) = 0;
    virtual void triangle(const ImVec2 &pos, const ImVec2 &size, const string &color, int orientation, const string &link) = 0;
    virtual void circle(const ImVec2 &pos, float radius) = 0;
    virtual void arrow(const ImVec2 &pos, float rotation, int orientation) = 0;
    virtual void line(const Line &line) = 0;
    virtual void dasharray(const Line &line) = 0;
    virtual void text(const ImVec2 &pos, const char *name, const string &link) = 0;
    virtual void label(const ImVec2 &pos, const char *name) = 0;
    virtual void dot(const ImVec2 &pos, int orientation) = 0;
};

bool scaledSVG = false; // Draw scaled SVG files todo toggleable

struct SVGDevice : Device {
    SVGDevice(string file_name, float width, float height) : file_name(std::move(file_name)) {
        static const float scale = 0.5;
        stream << format(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {} {}")", width, height);
        stream << (scaledSVG ? R"( width="100%" height="100%">)" : format(R"( width="{}mm" height="{}mm">)", width * scale, height * scale));
    }

    ~SVGDevice() override {
        stream << "</svg>\n";
        FileIO::write(file_name, stream.str());
    }

    static string xml_sanitize(const string &name) {
        static std::map<char, string> replacements{{'<', "&lt;"}, {'>', "&gt;"}, {'\'', "&apos;"}, {'"', "&quot;"}, {'&', "&amp;"}};

        auto replaced_name = name;
        for (const auto &[c, replacement]: replacements) {
            replaced_name = replace(replaced_name, c, replacement);
        }
        return replaced_name;
    }

    void rect(const ImVec4 &rect, const string &color, const string &link) override {
        if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link)); // open the optional link tag
        const auto [x, y, w, h] = rect;
        stream << format(R"(<rect x="{}" y="{}" width="{}" height="{}" rx="0" ry="0" style="stroke:none;fill:{};"/>)", x, y, w, h, color);
        if (!link.empty()) stream << "</a>"; // close the optional link tag
    }

    void triangle(const ImVec2 &pos, const ImVec2 &size, const string &color, int orientation, const string &link) override {
        if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link)); // open the optional link tag
        const auto [x, y] = pos;
        const auto [l, h] = size;

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

    void circle(const ImVec2 &pos, float radius) override {
        stream << format(R"(<circle cx="{}" cy="{}" r="{}"/>)", pos.x, pos.y, radius);
    }

    void arrow(const ImVec2 &pos, float rotation, int orientation) override {
        static const float dx = 3, dy = 1;
        const auto [x, y] = pos;
        const auto x1 = orientation == kLeftRight ? x - dx : x + dx;
        stream << rotate_line({{x1, y - dy}, pos}, rotation, x, y);
        stream << rotate_line({{x1, y + dy}, pos}, rotation, x, y);
    }

    void line(const Line &line) override {
        const auto &[start, end] = line;
        stream << format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}"  style="stroke:black; stroke-linecap:round; stroke-width:0.25;"/>)", start.x, start.y, end.x, end.y);
    }

    void dasharray(const Line &line) override {
        const auto &[start, end] = line;
        stream << format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}"  style="stroke: black; stroke-linecap:round; stroke-width:0.25; stroke-dasharray:3,3;"/>)", start.x, start.y, end.x, end.y);
    }

    void text(const ImVec2 &pos, const char *name, const string &link) override {
        if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link)); // open the optional link tag
        stream << format(R"(<text x="{}" y="{}" font-family="Arial" font-size="7" text-anchor="middle" fill="#FFFFFF">{}</text>)", pos.x, pos.y + 2, xml_sanitize(name));
        if (!link.empty()) stream << "</a>"; // close the optional link tag
    }

    void label(const ImVec2 &pos, const char *name) override {
        stream << format(R"(<text x="{}" y="{}" font-family="Arial" font-size="7">{}</text>)", pos.x, pos.y + 2, xml_sanitize(name));
    }

    void dot(const ImVec2 &pos, int orientation) override {
        const float offset = orientation == kLeftRight ? 2 : -2;
        stream << format(R"(<circle cx="{}" cy="{}" r="1"/>)", pos.x + offset, pos.y + offset);
    }

    static string rotate_line(const Line &line, float rx, float ry, float rz) {
        const auto &[start, end] = line;
        return format(R"lit(<line x1="{}" y1="{}" x2="{}" y2="{}" transform="rotate({},{},{})" style="stroke: black; stroke-width:0.25;"/>)lit", start.x, start.y, end.x, end.y, rx, ry, rz);
    }

private:
    string file_name;
    std::stringstream stream;
};

// An abstract block diagram schema
struct Schema {
    const unsigned int inputs, outputs;
    const float width, height;

    // Fields populated in `place()`:
    float x = 0, y = 0;
    int orientation = 0;

    std::vector<Line> lines; // Populated in `collectLines()`

    Schema(unsigned int inputs, unsigned int outputs, float width, float height) : inputs(inputs), outputs(outputs), width(width), height(height) {}
    virtual ~Schema() = default;

    void place(float new_x, float new_y, int new_orientation) {
        x = new_x;
        y = new_y;
        orientation = new_orientation;
        placeImpl();
    }

    void draw(Device &device) const {
        for (const auto &line: lines) { device.line(line); }
        drawImpl(device);
    }

    // abstract interface for subclasses
    virtual void placeImpl() = 0;
    virtual ImVec2 inputPoint(unsigned int i) const = 0;
    virtual ImVec2 outputPoint(unsigned int i) const = 0;
    virtual void collectLines() {}; // optional
    virtual void drawImpl(Device &) const {}; // optional
};

const float dWire = 8; // distance between two wires
const float dLetter = 4.3; // width of a letter
const float dHorz = 4;
const float dVert = 4;

struct IOSchema : Schema {
    IOSchema(unsigned int inputs, unsigned int outputs, float width, float height) : Schema(inputs, outputs, width, height) {
        for (unsigned int i = 0; i < inputs; i++) inputPoints.emplace_back(0, 0);
        for (unsigned int i = 0; i < outputs; i++) outputPoints.emplace_back(0, 0);
    }

    void placeImpl() override {
        const bool isLR = orientation == kLeftRight;
        const float dir = isLR ? dWire : -dWire;
        const float yMid = y + height / 2.0f;
        for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = {isLR ? x : x + width, yMid - dWire * float(inputs - 1) / 2.0f + float(i) * dir};
        for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = {isLR ? x + width : x, yMid - dWire * float(outputs - 1) / 2.0f + float(i) * dir};
    }

    ImVec2 inputPoint(unsigned int i) const override { return inputPoints[i]; }
    ImVec2 outputPoint(unsigned int i) const override { return outputPoints[i]; }

    std::vector<ImVec2> inputPoints;
    std::vector<ImVec2> outputPoints;
};

struct BinarySchema : Schema {
    BinarySchema(Schema *s1, Schema *s2, unsigned int inputs, unsigned int outputs, float width, float height)
        : Schema(inputs, outputs, width, height), schema1(s1), schema2(s2) {}

    ImVec2 inputPoint(unsigned int i) const override { return schema1->inputPoint(i); }
    ImVec2 outputPoint(unsigned int i) const override { return schema2->outputPoint(i); }

    void drawImpl(Device &device) const override {
        schema1->draw(device);
        schema2->draw(device);
    }

    void collectLines() override {
        schema1->collectLines();
        schema2->collectLines();
    }

    Schema *schema1;
    Schema *schema2;
};

// A simple rectangular box with a text and inputs and outputs.
struct BlockSchema : IOSchema {
    BlockSchema(unsigned int inputs, unsigned int outputs, float width, float height, string text, string color, string link)
        : IOSchema(inputs, outputs, width, height), text(std::move(text)), color(std::move(color)), link(std::move(link)) {}

    void drawImpl(Device &device) const override {
        device.rect(ImVec4{x, y, width, height} + ImVec4{dHorz, dVert, -2 * dHorz, -2 * dVert}, color, link);
        device.text(ImVec2{x, y} + ImVec2{width, height} / 2, text.c_str(), link);

        // Draw a small point that indicates the first input (like an integrated circuits).
        const bool isLR = orientation == kLeftRight;
        device.dot(ImVec2{x, y} + (isLR ? ImVec2{dHorz, dVert} : ImVec2{width - dHorz, height - dVert}), orientation);

        // Input arrows
        for (const auto &p: inputPoints) device.arrow(p + ImVec2{isLR ? dHorz : -dHorz, 0}, 0, orientation);
    }

    // Input/output wires
    void collectLines() override {
        const float dx = orientation == kLeftRight ? dHorz : -dHorz;
        for (const auto &p: inputPoints) lines.push_back({p, {p.x + dx, p.y}});
        for (const auto &p: outputPoints) lines.push_back({{p.x - dx, p.y}, p});
    }

    const string text, color, link;
};

static inline float quantize(int n) {
    static const int q = 3;
    return float(q * ((n + q - 1) / q)); // NOLINT(bugprone-integer-division)
}

// Simple cables (identity box) in parallel.
// The width of a cable is null.
// Therefor, input and output connection points are the same.
struct CableSchema : Schema {
    CableSchema(unsigned int n) : Schema(n, n, 0, float(n) * dWire) {}

    // Place the communication points vertically spaced by `dWire`.
    void placeImpl() override {
        for (unsigned int i = 0; i < inputs; i++) {
            const float dx = dWire * (float(i) + 0.5f);
            points[i] = {x, y + (orientation == kLeftRight ? dx : height - dx)};
        }
    }

    ImVec2 inputPoint(unsigned int i) const override { return points[i]; }
    ImVec2 outputPoint(unsigned int i) const override { return points[i]; }

private:
    std::vector<ImVec2> points{inputs};
};

// An inverter is a special symbol corresponding to '*(-1)' to create more compact diagrams.
struct InverterSchema : BlockSchema {
    InverterSchema(const string &color) : BlockSchema(1, 1, 2.5f * dWire, dWire, "-1", color, "") {}

    void drawImpl(Device &device) const override {
        device.triangle({x + dHorz, y + 0.5f}, {width - 2 * dHorz, height - 1}, color, orientation, link);
    }
};

// Terminate a cable (cut box).
struct CutSchema : Schema {
    // A Cut is represented by a small black dot.
    // It has 1 input and no outputs.
    // It has a 0 width and a 1 wire height.
    CutSchema() : Schema(1, 0, 0, dWire / 100.0f), point(0, 0) {}

    // The input point is placed in the middle.
    void placeImpl() override { point = {x, y + height * 0.5f}; }

    // A cut is represented by a small black dot.
    void drawImpl(Device &) const override {
        //    device.circle(point, dWire / 8.0);
    }

    // By definition, a Cut has only one input point.
    ImVec2 inputPoint(unsigned int) const override { return point; }

    // By definition, a Cut has no output point.
    ImVec2 outputPoint(unsigned int) const override {
        fgassert(false);
        return {-1, -1};
    }

private:
    ImVec2 point;
};

struct EnlargedSchema : IOSchema {
    EnlargedSchema(Schema *s, float width) : IOSchema(s->inputs, s->outputs, width, s->height), schema(s) {}

    void placeImpl() override {
        float dx = (width - schema->width) / 2;
        schema->place(x + dx, y, orientation);

        if (orientation == kRightLeft) dx = -dx;

        for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = schema->inputPoint(i) - ImVec2{dx, 0};
        for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = schema->outputPoint(i) + ImVec2{dx, 0};
    }

    void drawImpl(Device &device) const override { schema->draw(device); }

    void collectLines() override {
        schema->collectLines();
        for (unsigned int i = 0; i < inputs; i++) lines.emplace_back(inputPoint(i), schema->inputPoint(i));
        for (unsigned int i = 0; i < outputs; i++) lines.emplace_back(schema->outputPoint(i), outputPoint(i));
    }

private:
    Schema *schema;
};

struct ParallelSchema : BinarySchema {
    ParallelSchema(Schema *s1, Schema *s2)
        : BinarySchema(s1, s2, s1->inputs + s2->inputs, s1->outputs + s2->outputs, s1->width, s1->height + s2->height),
          inputFrontier(s1->inputs), outputFrontier(s1->outputs) {
        fgassert(s1->width == s2->width);
    }

    void placeImpl() override {
        if (orientation == kLeftRight) {
            schema1->place(x, y, orientation);
            schema2->place(x, y + schema1->height, orientation);
        } else {
            schema2->place(x, y, orientation);
            schema1->place(x, y + schema2->height, orientation);
        }
    }

    ImVec2 inputPoint(unsigned int i) const override {
        return i < inputFrontier ? schema1->inputPoint(i) : schema2->inputPoint(i - inputFrontier);
    }

    ImVec2 outputPoint(unsigned int i) const override {
        return i < outputFrontier ? schema1->outputPoint(i) : schema2->outputPoint(i - outputFrontier);
    }

private:
    unsigned int inputFrontier;
    unsigned int outputFrontier;
};

enum { kHorDir, kUpDir, kDownDir };  // directions of connections

// Compute the direction of a connection. Note that
// Y axis goes from top to bottom
static int direction(const ImVec2 &a, const ImVec2 &b) {
    if (a.y > b.y) return kUpDir;    // upward connections
    if (a.y < b.y) return kDownDir;  // downward connection
    return kHorDir;                  // horizontal connections
}

// Compute the horizontal gap needed to draw the internal wires.
// It depends on the largest group of connections that go in the same direction.
// May add cables to ensure the internal connections are between the same number of outputs and inputs.
static float computeHorzGap(Schema *a, Schema *b) {
    fgassert(a->outputs == b->inputs);

    if (a->outputs == 0) return 0;

    a->place(0, max(0.0f, 0.5f * (b->height - a->height)), kLeftRight);
    b->place(0, max(0.0f, 0.5f * (a->height - b->height)), kLeftRight);

    // todo simplify
    // init current group direction and size
    int gdir = direction(a->outputPoint(0), b->inputPoint(0));
    int gsize = 1;

    int MaxGroupSize[3] = {0, 0, 0}; // store the size of the largest group for each direction
    // analyze direction of remaining points
    for (unsigned int i = 1; i < a->outputs; i++) {
        int d = direction(a->outputPoint(i), b->inputPoint(i));
        if (d == gdir) {
            gsize++;
        } else {
            MaxGroupSize[gdir] = max(MaxGroupSize[gdir], gsize);
            gsize = 1;
            gdir = d;
        }
    }

    // update for last group
    if (gsize > MaxGroupSize[gdir]) MaxGroupSize[gdir] = gsize;

    // the gap required for the connections
    return dWire * float(max(MaxGroupSize[kUpDir], MaxGroupSize[kDownDir]));
}

struct SequentialSchema : BinarySchema {
    // Constructor for a sequential schema (s1:s2).
    // The components s1 and s2 are supposed to be "compatible" (s1 : n->m and s2 : m->q).
    SequentialSchema(Schema *s1, Schema *s2, float horzGap)
        : BinarySchema(s1, s2, s1->inputs, s2->outputs, s1->width + horzGap + s2->width, max(s1->height, s2->height)), horzGap(horzGap) {
        fgassert(s1->outputs == s2->inputs);
    }

    // Place the two components horizontally with enough space for the connections.
    void placeImpl() override {
        const float y1 = max(0.0f, 0.5f * (schema2->height - schema1->height));
        const float y2 = max(0.0f, 0.5f * (schema1->height - schema2->height));
        if (orientation == kLeftRight) {
            schema1->place(x, y + y1, orientation);
            schema2->place(x + schema1->width + horzGap, y + y2, orientation);
        } else {
            schema2->place(x, y + y2, orientation);
            schema1->place(x + schema2->width + horzGap, y + y1, orientation);
        }
    }

    void collectLines() override {
        BinarySchema::collectLines();
        collectInternalWires();
    }

private:
    // Draw the internal wires aligning the vertical segments in a symmetric way when possible.
    void collectInternalWires() {
        const unsigned int N = schema1->outputs;
        fgassert(N == schema2->inputs);

        float dx = 0, mx = 0;
        int dir = -1;
        for (unsigned int i = 0; i < N; i++) {
            const auto src = schema1->outputPoint(i);
            const auto dst = schema2->inputPoint(i);
            const int d = direction(src, dst);
            if (d == dir) {
                mx += dx; // move in same direction
            } else {
                mx = orientation == kLeftRight ? (d == kDownDir ? horzGap : 0) : (d == kUpDir ? -horzGap : 0);
                dx = d == kUpDir ? dWire : d == kDownDir ? -dWire : 0;
                dir = d;
            }
            // todo add a toggle to always draw the straight cable - I tried this and it can look better imo (diagonal lines instead of manhatten)
            if (src.y == dst.y) {
                // Draw a straight, potentially diagonal cable.
                lines.emplace_back(src, dst);
            } else {
                // Draw a zigzag cable by traversing half the distance between, taking a sharp turn, then turning back and finishing.
                lines.push_back({src, {src.x + mx, src.y}});
                lines.push_back({{src.x + mx, src.y}, {src.x + mx, dst.y}});
                lines.push_back({{src.x + mx, dst.y}, dst});
            }
        }
    }

    float horzGap;
};

// Place and connect two diagrams in merge composition.
struct MergeSchema : BinarySchema {
    // Constructor for a merge schema s1 :> s2 where the outputs of s1 are merged to the inputs of s2.
    MergeSchema(Schema *s1, Schema *s2, float horzGap)
        : BinarySchema(s1, s2, s1->inputs, s2->outputs, s1->width + s2->width + horzGap, max(s1->height, s2->height)), horzGap(horzGap) {}

    // Place the two subschema horizontally, centered, with enough gap for the connections.
    void placeImpl() override {
        const float dy1 = max(0.0f, schema2->height - schema1->height) / 2.0f;
        const float dy2 = max(0.0f, schema1->height - schema2->height) / 2.0f;
        if (orientation == kLeftRight) {
            schema1->place(x, y + dy1, orientation);
            schema2->place(x + schema1->width + horzGap, y + dy2, orientation);
        } else {
            schema2->place(x, y + dy2, orientation);
            schema1->place(x + schema2->width + horzGap, y + dy1, orientation);
        }
    }

    void collectLines() override {
        BinarySchema::collectLines();
        for (unsigned int i = 0; i < schema1->outputs; i++) lines.emplace_back(schema1->outputPoint(i), schema2->inputPoint(i % schema2->inputs));
    }

private:
    float horzGap;
};

// Place and connect two diagrams in split composition.
struct SplitSchema : BinarySchema {
    // Constructor for a split schema s1 <: s2, where the outputs of s1 are distributed to the inputs of s2.
    SplitSchema(Schema *s1, Schema *s2, float horzGap)
        : BinarySchema(s1, s2, s1->inputs, s2->outputs, s1->width + s2->width + horzGap, max(s1->height, s2->height)), horzGap(horzGap) {}

    // Place the two subschema horizontally, centered, with enough gap for the connections
    void placeImpl() override {
        const float dy1 = max(0.0f, schema2->height - schema1->height) / 2.0f;
        const float dy2 = max(0.0f, schema1->height - schema2->height) / 2.0f;
        if (orientation == kLeftRight) {
            schema1->place(x, y + dy1, orientation);
            schema2->place(x + schema1->width + horzGap, y + dy2, orientation);
        } else {
            schema2->place(x, y + dy2, orientation);
            schema1->place(x + schema2->width + horzGap, y + dy1, orientation);
        }
    }

    void collectLines() override {
        BinarySchema::collectLines();
        for (unsigned int i = 0; i < schema2->inputs; i++) lines.emplace_back(schema1->outputPoint(i % schema1->outputs), schema2->inputPoint(i));
    }

private:
    float horzGap;
};

// Place and connect two diagrams in recursive composition
// The two components must have the same width.
struct RecSchema : IOSchema {
    RecSchema(Schema *s1, Schema *s2, float width)
        : IOSchema(s1->inputs - s2->outputs, s1->outputs, width, s1->height + s2->height), schema1(s1), schema2(s2) {
        fgassert(s1->inputs >= s2->outputs);
        fgassert(s1->outputs >= s2->inputs);
        fgassert(s1->width >= s2->width);
    }

    // The two subschema are placed centered vertically, s2 on top of s1.
    void placeImpl() override {
        float dx1 = (width - schema1->width) / 2;
        const float dx2 = (width - schema2->width) / 2;
        if (orientation == kLeftRight) {
            schema2->place(x + dx2, y, kRightLeft);
            schema1->place(x + dx1, y + schema2->height, kLeftRight);
        } else {
            schema1->place(x + dx1, y, kRightLeft);
            schema2->place(x + dx2, y + schema1->height, kLeftRight);
        }

        const ImVec2 d1 = {orientation == kRightLeft ? -dx1 : dx1, 0};
        for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = schema1->inputPoint(i + schema2->outputs) - d1;
        for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = schema1->outputPoint(i) + d1;
    }

    // Draw the delay sign of a feedback connection
    static void drawDelaySign(Device &device, float x, float y, float size) {
        device.line({{x - size / 2, y}, {x - size / 2, y - size}});
        device.line({{x - size / 2, y - size}, {x + size / 2, y - size}});
        device.line({{x + size / 2, y - size}, {x + size / 2, y}});
    }

    void drawImpl(Device &device) const override {
        schema1->draw(device);
        schema2->draw(device);

        // Draw the implicit feedback delay to each schema2 input
        const float dw = orientation == kLeftRight ? dWire : -dWire;
        for (unsigned int i = 0; i < schema2->inputs; i++) {
            const auto &p = schema1->outputPoint(i) + ImVec2{float(i) * dw, 0};
            drawDelaySign(device, p.x, p.y, dw / 2);
        }
    }

    void collectLines() override {
        schema1->collectLines();
        schema2->collectLines();

        // Feedback connections to each schema2 input
        for (unsigned int i = 0; i < schema2->inputs; i++) collectFeedback(schema1->outputPoint(i), schema2->inputPoint(i), float(i) * dWire, outputPoint(i));
        // Non-recursive output lines
        for (unsigned int i = schema2->inputs; i < outputs; i++) lines.emplace_back(schema1->outputPoint(i), outputPoint(i));
        // Input lines
        for (unsigned int i = 0; i < inputs; i++) lines.emplace_back(inputPoint(i), schema1->inputPoint(i + schema2->outputs));
        // Feedfront connections from each schema2 output
        for (unsigned int i = 0; i < schema2->outputs; i++) collectFeedfront(schema2->outputPoint(i), schema1->inputPoint(i), float(i) * dWire);
    }

private:
    // Draw a feedback connection between two points with a horizontal displacement `dx`.
    void collectFeedback(const ImVec2 &src, const ImVec2 &dst, float dx, const ImVec2 &out) {
        const float ox = src.x + (orientation == kLeftRight ? dx : -dx);
        const float ct = (orientation == kLeftRight ? dWire : -dWire) / 2.0f;
        const ImVec2 up(ox, src.y - ct);
        const ImVec2 br(ox + ct / 2.0f, src.y);

        lines.push_back({up, {ox, dst.y}});
        lines.push_back({{ox, dst.y}, dst});
        lines.emplace_back(src, br);
        lines.emplace_back(br, out);
    }

    // Draw a feedfrom connection between two points with a horizontal displacement `dx`.
    void collectFeedfront(const ImVec2 &src, const ImVec2 &dst, float dx) {
        const float ox = src.x + (orientation == kLeftRight ? -dx : dx);
        lines.push_back({{src.x, src.y}, {ox, src.y}});
        lines.push_back({{ox, src.y}, {ox, dst.y}});
        lines.push_back({{ox, dst.y}, {dst.x, dst.y}});
    }

    Schema *schema1;
    Schema *schema2;
};

// A TopSchema is a schema surrounded by a dashed rectangle with a label on the top left.
// The rectangle is placed at half the margin parameter.
// Arrows are added to all the outputs.
struct TopSchema : Schema {
    // A TopSchema is a schema surrounded by a dashed rectangle with a label on the top left, and arrows added to the outputs.
    TopSchema(Schema *s, string link, string text, float margin)
        : Schema(0, 0, s->width + 2 * margin, s->height + 2 * margin), schema(s), text(std::move(text)), link(std::move(link)), margin(margin) {}

    void placeImpl() override { schema->place(x + margin, y + margin, orientation); }

    // Top schema has no input or output
    ImVec2 inputPoint(unsigned int) const override { throw std::runtime_error("ERROR : TopSchema::inputPoint"); }
    ImVec2 outputPoint(unsigned int) const override { throw std::runtime_error("ERROR : TopSchema::outputPoint"); }

    void drawImpl(Device &device) const override {
        device.rect({x, y, width - 1, height - 1}, "#ffffff", link);
        device.label(ImVec2{x, y} + ImVec2{margin, margin / 2}, text.c_str());

        schema->draw(device);

        for (unsigned int i = 0; i < schema->outputs; i++) device.arrow(schema->outputPoint(i), 0, orientation);
    }

    void collectLines() override { schema->collectLines(); }

private:
    Schema *schema;
    string text, link;
    float margin;
};

// A `DecorateSchema` is a schema surrounded by a dashed rectangle with a label on the top left.
struct DecorateSchema : IOSchema {
    DecorateSchema(Schema *s, string text, float margin)
        : IOSchema(s->inputs, s->outputs, s->width + 2 * margin, s->height + 2 * margin), schema(s), margin(margin), text(std::move(text)) {}

    void placeImpl() override {
        schema->place(x + margin, y + margin, orientation);

        const float m = orientation == kRightLeft ? -margin : margin;
        for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = schema->inputPoint(i) - ImVec2{m, 0};
        for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = schema->outputPoint(i) + ImVec2{m, 0};
    }

    void drawImpl(Device &device) const override {
        schema->draw(device);

        // Surrounding frame
        const auto topLeft = ImVec2{x, y} + ImVec2{margin, margin} / 2;
        const auto topRight = topLeft + ImVec2{width - margin, 0};
        const auto bottomLeft = topLeft + ImVec2{0, height - margin};
        const auto bottomRight = bottomLeft + ImVec2{width - margin, 0};
        const float textLeft = x + margin;

        device.dasharray({topLeft, bottomLeft}); // left line
        device.dasharray({bottomLeft, bottomRight}); // bottom line
        device.dasharray({bottomRight, topRight}); // right line
        device.dasharray({topLeft, {textLeft, topLeft.y}}); // top segment before text
        device.dasharray({{min(textLeft + float(2 + text.size()) * dLetter * 0.75f, bottomRight.x), topLeft.y}, {bottomRight.x, topLeft.y}}); // top segment after text

        device.label({textLeft, topLeft.y}, text.c_str());
    }

    void collectLines() override {
        schema->collectLines();
        for (unsigned int i = 0; i < inputs; i++) lines.emplace_back(inputPoint(i), schema->inputPoint(i));
        for (unsigned int i = 0; i < outputs; i++) lines.emplace_back(schema->outputPoint(i), outputPoint(i));
    }

private:
    Schema *schema;
    float margin;
    string text;
};

// A simple rectangular box with a text and inputs and outputs.
// A connector is an invisible square for `dWire` size with 1 input and 1 output.
struct ConnectorSchema : IOSchema {
    ConnectorSchema() : IOSchema(1, 1, dWire, dWire) {}

    void collectLines() override {
        const float dx = orientation == kLeftRight ? dHorz : -dHorz;
        for (const auto &p: inputPoints) lines.emplace_back(p, p + ImVec2{dx, 0});
        for (const auto &p: outputPoints) lines.emplace_back(p - ImVec2{dx, 0}, p);
    }
};

// A simple rectangular box with a text and inputs and outputs.
struct RouteSchema : IOSchema {
    // Build a simple colored `RouteSchema` with a certain number of inputs and outputs, a text to be displayed, and an optional link.
    // The length of the text as well as the number of inputs and outputs are used to compute the size of the `RouteSchema`
    RouteSchema(unsigned int inputs, unsigned int outputs, float width, float height, std::vector<int> routes)
        : IOSchema(inputs, outputs, width, height), color("#EEEEAA"), routes(std::move(routes)) {}

    void drawImpl(Device &device) const override {
        static bool drawRouteFrame = false; // todo provide toggle
        if (drawRouteFrame) {
            device.rect(ImVec4{x, y, width, height} + ImVec4{dHorz, dVert, -2 * dHorz, -2 * dVert}, color, link);
            // device.text(x + width / 2, y + height / 2, text.c_str(), link);

            // Draw the orientation mark, a small point that indicates the first input (like integrated circuits).
            const bool isLR = orientation == kLeftRight;
            device.dot(ImVec2{x, y} + (isLR ? ImVec2{dHorz, dVert} : ImVec2{width - dHorz, height - dVert}), orientation);

            // Input arrows
            for (const auto &p: inputPoints) device.arrow(p + ImVec2{isLR ? dHorz : -dHorz, 0}, 0, orientation);
        }
    }

    void collectLines() override {
        const float dx = orientation == kLeftRight ? dHorz : -dHorz;
        // Input/output wires
        for (const auto &p: inputPoints) lines.emplace_back(p, p + ImVec2{dx, 0});
        for (const auto &p: outputPoints) lines.emplace_back(p - ImVec2{dx, 0}, p);

        // Route wires
        for (unsigned int i = 0; i < routes.size() - 1; i += 2) {
            lines.emplace_back(inputPoints[routes[i] - 1] + ImVec2{dx, 0}, outputPoints[routes[i + 1] - 1] - ImVec2{dx, 0});
        }
    }

protected:
    const string text, color, link;
    const std::vector<int> routes;  // Route description: s1,d2,s2,d2,...
};

Schema *makeConnectorSchema() { return new ConnectorSchema(); }
Schema *makeBlockSchema(unsigned int inputs, unsigned int outputs, const string &text, const string &color, const string &link) {
    const float minimal = 3 * dWire;
    const float w = 2 * dHorz + max(minimal, dLetter * quantize(int(text.size())));
    const float h = 2 * dVert + max(minimal, float(max(inputs, outputs)) * dWire);
    return new BlockSchema(inputs, outputs, w, h, text, color, link);
}
Schema *makeDecorateSchema(Schema *s, const string &text, float margin = 10) { return new DecorateSchema(s, text, margin); }
Schema *makeTopSchema(Schema *s, const string &link, const string &text, float margin = 10) { return new TopSchema(makeDecorateSchema(s, text, margin), link, "", margin); }
Schema *makeCableSchema(unsigned int n = 1) { return new CableSchema(n); }
Schema *makeInverterSchema(const string &color) { return new InverterSchema(color); }
Schema *makeCutSchema() { return new CutSchema(); }
Schema *makeEnlargedSchema(Schema *s, float width) { return width > s->width ? new EnlargedSchema(s, width) : s; }
Schema *makeParallelSchema(Schema *s1, Schema *s2) { return new ParallelSchema(makeEnlargedSchema(s1, s2->width), makeEnlargedSchema(s2, s1->width)); }
Schema *makeSequentialSchema(Schema *s1, Schema *s2) {
    const unsigned int o = s1->outputs;
    const unsigned int i = s2->inputs;
    auto *a = o < i ? makeParallelSchema(s1, makeCableSchema(i - o)) : s1;
    auto *b = o > i ? makeParallelSchema(s2, makeCableSchema(o - i)) : s2;

    return new SequentialSchema(a, b, computeHorzGap(a, b));
}
Schema *makeMergeSchema(Schema *s1, Schema *s2) {
    auto *a = makeEnlargedSchema(s1, dWire);
    auto *b = makeEnlargedSchema(s2, dWire);
    return new MergeSchema(a, b, (a->height + b->height) / 4); // Horizontal gap to avoid sloppy connections.
}
Schema *makeSplitSchema(Schema *s1, Schema *s2) {
    auto *a = makeEnlargedSchema(s1, dWire);
    auto *b = makeEnlargedSchema(s2, dWire);
    return new SplitSchema(a, b, (a->height + b->height) / 4); // Horizontal gap to avoid sloppy connections.
}

Schema *makeRecSchema(Schema *s1, Schema *s2) {
    auto *a = makeEnlargedSchema(s1, s2->width);
    auto *b = makeEnlargedSchema(s2, s1->width);
    // The smaller component is enlarged to the width of the other.
    const float w = a->width + 2 * (dWire * float(max(b->inputs, b->outputs)));
    return new RecSchema(a, b, w);
}

// Build n x m cable routing
Schema *makeRouteSchema(unsigned int inputs, unsigned int outputs, const std::vector<int> &routes) {
    const float minimal = 3 * dWire;
    const float h = 2 * dVert + max(minimal, max(float(inputs), float(outputs)) * dWire);
    const float w = 2 * dHorz + max(minimal, h * 0.75f);
    return new RouteSchema(inputs, outputs, w, h, routes);
}

#define linkcolor "#003366"
#define normalcolor "#4B71A1"
#define uicolor "#477881"
#define slotcolor "#47945E"
#define numcolor "#f44800"
#define invcolor "#ffffff"

struct DrawContext {
    Tree boxComplexityMemo{}; // Avoid recomputing box complexity
    property<bool> pureRoutingPropertyMemo{}; // Avoid recomputing pure-routing property
    string schemaFileName;  // Name of schema file being generated
    std::set<Tree> drawnExp; // Expressions drawn or scheduled so far
    std::map<Tree, string> backLink; // Link to enclosing file for sub schema
    std::stack<Tree> pendingExp; // Expressions that need to be drawn
    bool foldingFlag = false; // true with complex block-diagrams
};

std::unique_ptr<DrawContext> dc;

static int computeComplexity(Box box);

// Memoized version of `computeComplexity(Box)`
int boxComplexity(Box box) {
    Tree prop = box->getProperty(dc->boxComplexityMemo);
    if (prop) return tree2int(prop);

    int v = computeComplexity(box);
    box->setProperty(dc->boxComplexityMemo, tree(v));
    return v;
}

// Compute the complexity of a box expression tree according to the complexity of its subexpressions.
// Basically, it counts the number of boxes to be drawn.
// If the box-diagram expression is not evaluated, it will throw an error.
int computeComplexity(Box box) {
    if (isBoxCut(box) || isBoxWire(box)) return 0;

    int i;
    double r;
    prim0 p0;
    prim1 p1;
    prim2 p2;
    prim3 p3;
    prim4 p4;
    prim5 p5;

    const auto *xt = getUserData(box);

    // simple elements / slot
    if (xt ||
        isBoxInt(box, &i) ||
        isBoxReal(box, &r) ||
        isBoxWaveform(box) ||
        isBoxPrim0(box, &p0) ||
        isBoxPrim1(box, &p1) ||
        isBoxPrim2(box, &p2) ||
        isBoxPrim3(box, &p3) ||
        isBoxPrim4(box, &p4) ||
        isBoxPrim5(box, &p5) ||
        isBoxSlot(box, &i))
        return 1;

    Tree ff, type, name, file;
    // foreign elements
    if (isBoxFFun(box, ff) ||
        isBoxFConst(box, type, name, file) ||
        isBoxFVar(box, type, name, file))
        return 1;

    Tree t1, t2;

    // symbolic boxes
    if (isBoxSymbolic(box, t1, t2)) return 1 + boxComplexity(t2);

    // binary operators
    if (isBoxSeq(box, t1, t2) ||
        isBoxSplit(box, t1, t2) ||
        isBoxMerge(box, t1, t2) ||
        isBoxPar(box, t1, t2) ||
        isBoxRec(box, t1, t2))
        return boxComplexity(t1) + boxComplexity(t2);

    Tree label, cur, min, max, step, chan;

    // user interface widgets
    if (isBoxButton(box, label) ||
        isBoxCheckbox(box, label) ||
        isBoxVSlider(box, label, cur, min, max, step) ||
        isBoxHSlider(box, label, cur, min, max, step) ||
        isBoxHBargraph(box, label, min, max) ||
        isBoxVBargraph(box, label, min, max) ||
        isBoxSoundfile(box, label, chan) ||
        isBoxNumEntry(box, label, cur, min, max, step))
        return 1;

    // user interface groups
    if (isBoxVGroup(box, label, t1) ||
        isBoxHGroup(box, label, t1) ||
        isBoxTGroup(box, label, t1) ||
        isBoxMetadata(box, t1, t2))
        return boxComplexity(t1);

    Tree t3;
    // environment/route
    if (isBoxEnvironment(box) || isBoxRoute(box, t1, t2, t3)) return 0;

    stringstream error;
    error << "ERROR in boxComplexity : not an evaluated box [[ " << *box << " ]]\n";
    throw std::runtime_error(error.str());
}

static Schema *createSchema(Tree t);

// Generate a 1->0 block schema for an input slot.
static Schema *generateInputSlotSchema(Tree a) {
    Tree id;
    getDefNameProperty(a, id);
    return makeBlockSchema(1, 0, tree2str(id), slotcolor, "");
}
// Generate an abstraction schema by placing in sequence the input slots and the body.
static Schema *generateAbstractionSchema(Schema *x, Tree t) {
    Tree a, b;
    while (isBoxSymbolic(t, a, b)) {
        x = makeParallelSchema(x, generateInputSlotSchema(a));
        t = b;
    }
    return makeSequentialSchema(x, createSchema(t));
}

static Schema *addSchemaInputs(int ins, Schema *x) {
    if (ins == 0) return x;

    Schema *y = nullptr;
    do {
        Schema *z = makeConnectorSchema();
        y = y != nullptr ? makeParallelSchema(y, z) : z;
    } while (--ins);

    return makeSequentialSchema(y, x);
}
static Schema *addSchemaOutputs(int outs, Schema *x) {
    if (outs == 0) return x;

    Schema *y = nullptr;
    do {
        Schema *z = makeConnectorSchema();
        y = y != nullptr ? makeParallelSchema(y, z) : z;
    } while (--outs);

    return makeSequentialSchema(x, y);
}

// Transform the definition name property of tree <t> into a legal file name.
// The resulting file name is stored in <dst> a table of at least <n> chars.
// Returns the <dst> pointer for convenience.
static string legalFileName(Tree t, const string &id) {
    const string dst = views::take_while(id, [](char c) { return std::isalnum(c); }) | views::take(16) | to<string>();
    // if it is not process add the hex address to make the name unique
    return dst != "process" ? dst + format("-{:p}", (void *) t) : dst;
}

// Returns `true` if `t == '*(-1)'`.
// This test is used to simplify diagram by using a special symbol for inverters.
static bool isInverter(Tree t) {
    static Tree inverters[6]{
        boxSeq(boxPar(boxWire(), boxInt(-1)), boxPrim2(sigMul)),
        boxSeq(boxPar(boxInt(-1), boxWire()), boxPrim2(sigMul)),
        boxSeq(boxPar(boxWire(), boxReal(-1.0)), boxPrim2(sigMul)),
        boxSeq(boxPar(boxReal(-1.0), boxWire()), boxPrim2(sigMul)),
        boxSeq(boxPar(boxInt(0), boxWire()), boxPrim2(sigSub)),
        boxSeq(boxPar(boxReal(0.0), boxWire()), boxPrim2(sigSub)),
    };
    return ::ranges::contains(inverters, t);
}

// Collect the leaf numbers of tree l into vector v.
// Return true if l a number or a parallel tree of numbers.
static bool isIntTree(Tree l, std::vector<int> &v) {
    int n;
    if (isBoxInt(l, &n)) {
        v.push_back(n);
        return true;
    }

    double r;
    if (isBoxReal(l, &r)) {
        v.push_back(int(r));
        return true;
    }

    Tree x, y;
    if (isBoxPar(l, x, y)) return isIntTree(x, v) && isIntTree(y, v);

    throw std::runtime_error((stringstream("ERROR in file ") << __FILE__ << ':' << __LINE__ << ", not a valid list of numbers : " << boxpp(l)).str());
}

// Convert user interface element into a textual representation
static string userInterfaceDescription(Tree box) {
    Tree t1, label, cur, min, max, step, chan;
    if (isBoxButton(box, label)) return "button(" + extractName(label) + ')';
    if (isBoxCheckbox(box, label)) return "checkbox(" + extractName(label) + ')';
    if (isBoxVSlider(box, label, cur, min, max, step)) return (stringstream("vslider(") << extractName(label) << ", " << boxpp(cur) << ", " << boxpp(min) << ", " << boxpp(max) << ", " << boxpp(step) << ')').str();
    if (isBoxHSlider(box, label, cur, min, max, step)) return (stringstream("hslider(") << extractName(label) << ", " << boxpp(cur) << ", " << boxpp(min) << ", " << boxpp(max) << ", " << boxpp(step) << ')').str();
    if (isBoxVGroup(box, label, t1)) return (stringstream("vgroup(") << extractName(label) << ", " << boxpp(t1, 0) << ')').str();
    if (isBoxHGroup(box, label, t1)) return (stringstream("hgroup(") << extractName(label) << ", " << boxpp(t1, 0) << ')').str();
    if (isBoxTGroup(box, label, t1)) return (stringstream("tgroup(") << extractName(label) << ", " << boxpp(t1, 0) << ')').str();
    if (isBoxHBargraph(box, label, min, max)) return (stringstream("hbargraph(") << extractName(label) << ", " << boxpp(min) << ", " << boxpp(max) << ')').str();
    if (isBoxVBargraph(box, label, min, max)) return (stringstream("vbargraph(") << extractName(label) << ", " << boxpp(min) << ", " << boxpp(max) << ')').str();
    if (isBoxNumEntry(box, label, cur, min, max, step)) return (stringstream("nentry(") << extractName(label) << ", " << boxpp(cur) << ", " << boxpp(min) << ", " << boxpp(max) << ", " << boxpp(step) << ')').str();
    if (isBoxSoundfile(box, label, chan)) return (stringstream("soundfile(") << extractName(label) << ", " << boxpp(chan) << ')').str();

    throw std::runtime_error("ERROR : unknown user interface element");
}

// Generate the inside schema of a block diagram according to its type.
static Schema *generateInsideSchema(Tree t) {
    if (getUserData(t) != nullptr) return makeBlockSchema(xtendedArity(t), 1, xtendedName(t), normalcolor, "");
    if (isInverter(t)) return makeInverterSchema(invcolor);

    int i;
    double r;
    if (isBoxInt(t, &i) || isBoxReal(t, &r)) {
        stringstream s;
        if (isBoxInt(t)) s << i;
        else s << r;
        return makeBlockSchema(0, 1, s.str(), numcolor, "");
    }

    if (isBoxWaveform(t)) return makeBlockSchema(0, 2, "waveform{...}", normalcolor, "");
    if (isBoxWire(t)) return makeCableSchema();
    if (isBoxCut(t)) return makeCutSchema();

    prim0 p0;
    prim1 p1;
    prim2 p2;
    prim3 p3;
    prim4 p4;
    prim5 p5;
    if (isBoxPrim0(t, &p0)) return makeBlockSchema(0, 1, prim0name(p0), normalcolor, "");
    if (isBoxPrim1(t, &p1)) return makeBlockSchema(1, 1, prim1name(p1), normalcolor, "");
    if (isBoxPrim2(t, &p2)) return makeBlockSchema(2, 1, prim2name(p2), normalcolor, "");
    if (isBoxPrim3(t, &p3)) return makeBlockSchema(3, 1, prim3name(p3), normalcolor, "");
    if (isBoxPrim4(t, &p4)) return makeBlockSchema(4, 1, prim4name(p4), normalcolor, "");
    if (isBoxPrim5(t, &p5)) return makeBlockSchema(5, 1, prim5name(p5), normalcolor, "");

    Tree ff;
    if (isBoxFFun(t, ff)) return makeBlockSchema(ffarity(ff), 1, ffname(ff), normalcolor, "");

    Tree label, chan, type, name, file;
    if (isBoxFConst(t, type, name, file) || isBoxFVar(t, type, name, file)) return makeBlockSchema(0, 1, tree2str(name), normalcolor, "");
    if (isBoxButton(t) || isBoxCheckbox(t) || isBoxVSlider(t) || isBoxHSlider(t) || isBoxNumEntry(t)) return makeBlockSchema(0, 1, userInterfaceDescription(t), uicolor, "");
    if (isBoxVBargraph(t) || isBoxHBargraph(t)) return makeBlockSchema(1, 1, userInterfaceDescription(t), uicolor, "");
    if (isBoxSoundfile(t, label, chan)) return makeBlockSchema(2, 2 + tree2int(chan), userInterfaceDescription(t), uicolor, "");

    Tree a, b;
    if (isBoxMetadata(t, a, b)) return createSchema(a);

    const bool isVGroup = isBoxVGroup(t, label, a);
    const bool isHGroup = isBoxHGroup(t, label, a);
    const bool isTGroup = isBoxTGroup(t, label, a);
    if (isVGroup || isHGroup || isTGroup) {
        const string groupId = isVGroup ? "v" : isHGroup ? "h" : "t";
        auto *s1 = createSchema(a);
        return makeDecorateSchema(s1, groupId + "group(" + extractName(label) + ")");
    }
    if (isBoxSeq(t, a, b)) return makeSequentialSchema(createSchema(a), createSchema(b));
    if (isBoxPar(t, a, b)) return makeParallelSchema(createSchema(a), createSchema(b));
    if (isBoxSplit(t, a, b)) return makeSplitSchema(createSchema(a), createSchema(b));
    if (isBoxMerge(t, a, b)) return makeMergeSchema(createSchema(a), createSchema(b));
    if (isBoxRec(t, a, b)) return makeRecSchema(createSchema(a), createSchema(b));
    if (isBoxSlot(t, &i)) {
        Tree id;
        getDefNameProperty(t, id);
        return makeBlockSchema(0, 1, tree2str(id), slotcolor, "");
    }
    if (isBoxSymbolic(t, a, b)) {
        auto *inputSlotSchema = generateInputSlotSchema(a);
        auto *abstractionSchema = generateAbstractionSchema(inputSlotSchema, b);

        Tree id;
        if (getDefNameProperty(t, id)) return abstractionSchema;
        return makeDecorateSchema(abstractionSchema, "Abstraction");
    }
    if (isBoxEnvironment(t)) return makeBlockSchema(0, 0, "environment{...}", normalcolor, "");

    Tree c;
    if (isBoxRoute(t, a, b, c)) {
        int ins, outs;
        vector<int> route;
        if (isBoxInt(a, &ins) && isBoxInt(b, &outs) && isIntTree(c, route)) return makeRouteSchema(ins, outs, route);

        throw std::runtime_error((stringstream("ERROR in file ") << __FILE__ << ':' << __LINE__ << ", invalid route expression : " << boxpp(t)).str());
    }

    throw std::runtime_error((stringstream("ERROR in generateInsideSchema, box expression not recognized: ") << boxpp(t)).str());
}

// TODO provide controls for these properties
const int foldThreshold = 25; // global complexity threshold before activating folding
const int foldComplexity = 2; // individual complexity threshold before folding
const fs::path faustDiagramsPath = "FaustDiagrams"; // todo properties

// Write a top level diagram.
// A top level diagram is decorated with its definition name property and is drawn in an individual file.
static void writeSchemaFile(Tree bd) {
    int ins, outs;
    getBoxType(bd, &ins, &outs);

    Tree idTree;
    getDefNameProperty(bd, idTree);
    const string &id = tree2str(idTree);
    dc->schemaFileName = legalFileName(bd, id) + ".svg";

    auto *ts = makeTopSchema(addSchemaOutputs(outs, addSchemaInputs(ins, generateInsideSchema(bd))), dc->backLink[bd], id);
    // todo combine place/collect/draw
    ts->place(0, 0, kLeftRight);
    ts->collectLines();
    SVGDevice dev(faustDiagramsPath / dc->schemaFileName, ts->width, ts->height);
    ts->draw(dev);
}

// Schedule a block diagram to be drawn.
static void scheduleDrawing(Tree t) {
    if (dc->drawnExp.find(t) == dc->drawnExp.end()) {
        dc->drawnExp.insert(t);
        dc->backLink.emplace(t, dc->schemaFileName); // remember the enclosing filename
        dc->pendingExp.push(t);
    }
}

// Retrieve next block diagram that must be drawn.
static bool pendingDrawing(Tree &t) {
    if (dc->pendingExp.empty()) return false;
    t = dc->pendingExp.top();
    dc->pendingExp.pop();
    return true;
}

void drawBox(Box box) {
    fs::remove_all(faustDiagramsPath);
    fs::create_directory(faustDiagramsPath);

    dc = std::make_unique<DrawContext>();
    dc->foldingFlag = boxComplexity(box) > foldThreshold;

    scheduleDrawing(box); // schedule the initial drawing

    Tree t;
    while (pendingDrawing(t)) writeSchemaFile(t); // generate all the pending drawings
}

// Compute the Pure Routing property.
// That is, expressions only made of cut, wires and slots.
// No labels will be displayed for pure routing expressions.
static bool isPureRouting(Tree t) {
    bool r;
    int ID;
    Tree x, y;

    if (dc->pureRoutingPropertyMemo.get(t, r)) return r;

    if (isBoxCut(t) || isBoxWire(t) || isInverter(t) || isBoxSlot(t, &ID) ||
        (isBoxPar(t, x, y) && isPureRouting(x) && isPureRouting(y)) ||
        (isBoxSeq(t, x, y) && isPureRouting(x) && isPureRouting(y)) ||
        (isBoxSplit(t, x, y) && isPureRouting(x) && isPureRouting(y)) ||
        (isBoxMerge(t, x, y) && isPureRouting(x) && isPureRouting(y))) {
        dc->pureRoutingPropertyMemo.set(t, true);
        return true;
    }

    dc->pureRoutingPropertyMemo.set(t, false);
    return false;
}

// Generate an appropriate schema according to the type of block diagram.
static Schema *createSchema(Tree t) {
    Tree idTree;
    if (getDefNameProperty(t, idTree)) {
        const string &id = tree2str(idTree);
        if (dc->foldingFlag && boxComplexity(t) >= foldComplexity) {
            int ins, outs;
            getBoxType(t, &ins, &outs);
            scheduleDrawing(t);
            return makeBlockSchema(ins, outs, tree2str(idTree), linkcolor, legalFileName(t, id) + ".svg");
        }
        // Not a slot, with a name. Draw a line around the object with its name.
        if (!isPureRouting(t)) return makeDecorateSchema(generateInsideSchema(t), id);
    }

    return generateInsideSchema(t); // normal case
}
