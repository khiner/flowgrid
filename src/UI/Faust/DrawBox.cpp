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

static const bool scaledSVG = false; // Draw scaled SVG files todo toggleable
static const float binarySchemaHorizontalGapRatio = 4; // todo style prop
static const bool sequentialConnectionZigzag = true; // false allows for diagonal lines instead of zigzags instead of zigzags todo style prop
static const bool drawRouteFrame = false; // todo style prop
static const int foldThreshold = 25; // global complexity threshold before activating folding todo configurable in state
static const int foldComplexity = 2; // individual complexity threshold before folding todo configurable in state
static const fs::path faustDiagramsPath = "FaustDiagrams"; // todo app property

// todo move to FlowGridStyle::Colors
static const string LinkColor = "#003366";
static const string NormalColor = "#4b71a1";
static const string UiColor = "#477881";
static const string SlotColor = "#47945e";
static const string NumberColor = "#f44800";
static const string InverterColor = "#ffffff";

enum { kLeftRight = 1, kRightLeft = -1 };

class Device {
public:
    virtual ~Device() = default;
    virtual void rect(const ImVec4 &rect, const string &color, const string &link) = 0;
    virtual void triangle(const ImVec2 &pos, const ImVec2 &size, const string &color, int orientation, const string &link) = 0;
    virtual void circle(const ImVec2 &pos, float radius) = 0;
    virtual void arrow(const ImVec2 &pos, float rotation, int orientation) = 0;
    virtual void line(const ImVec2 &start, const ImVec2 &end) = 0;
    virtual void dasharray(const ImVec2 &start, const ImVec2 &end) = 0;
    virtual void text(const ImVec2 &pos, const string &name, const string &link) = 0;
    virtual void label(const ImVec2 &pos, const string &name) = 0;
    virtual void dot(const ImVec2 &pos, int orientation) = 0;
};

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
        stream << rotate_line({x1, y - dy}, pos, rotation, x, y);
        stream << rotate_line({x1, y + dy}, pos, rotation, x, y);
    }

    void line(const ImVec2 &start, const ImVec2 &end) override {
        stream << format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}"  style="stroke:black; stroke-linecap:round; stroke-width:0.25;"/>)", start.x, start.y, end.x, end.y);
    }

    void dasharray(const ImVec2 &start, const ImVec2 &end) override {
        stream << format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}"  style="stroke: black; stroke-linecap:round; stroke-width:0.25; stroke-dasharray:3,3;"/>)", start.x, start.y, end.x, end.y);
    }

    void text(const ImVec2 &pos, const string &name, const string &link) override {
        if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link)); // open the optional link tag
        stream << format(R"(<text x="{}" y="{}" font-family="Arial" font-size="7" text-anchor="middle" fill="#FFFFFF">{}</text>)", pos.x, pos.y + 2, xml_sanitize(name));
        if (!link.empty()) stream << "</a>"; // close the optional link tag
    }

    void label(const ImVec2 &pos, const string &name) override {
        stream << format(R"(<text x="{}" y="{}" font-family="Arial" font-size="7">{}</text>)", pos.x, pos.y + 2, xml_sanitize(name));
    }

    void dot(const ImVec2 &pos, int orientation) override {
        const float offset = orientation == kLeftRight ? 2 : -2;
        stream << format(R"(<circle cx="{}" cy="{}" r="1"/>)", pos.x + offset, pos.y + offset);
    }

    static string rotate_line(const ImVec2 &start, const ImVec2 &end, float rx, float ry, float rz) {
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
    int orientation = kLeftRight;

    Schema(unsigned int inputs, unsigned int outputs, float width, float height) : inputs(inputs), outputs(outputs), width(width), height(height) {}
    virtual ~Schema() = default;

    void place(float new_x, float new_y, int new_orientation) {
        x = new_x;
        y = new_y;
        orientation = new_orientation;
        placeImpl();
    }
    void place() { placeImpl(); }
    virtual ImVec2 inputPoint(unsigned int i) const = 0;
    virtual ImVec2 outputPoint(unsigned int i) const = 0;
    virtual void draw(Device &) const {};

protected:
    virtual void placeImpl() = 0;
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

// A simple rectangular box with a text and inputs and outputs.
struct BlockSchema : IOSchema {
    BlockSchema(unsigned int inputs, unsigned int outputs, float width, float height, string text, string color, string link = "")
        : IOSchema(inputs, outputs, width, height), text(std::move(text)), color(std::move(color)), link(std::move(link)) {}

    void draw(Device &device) const override {
        device.rect(ImVec4{x, y, width, height} + ImVec4{dHorz, dVert, -2 * dHorz, -2 * dVert}, color, link);
        device.text(ImVec2{x, y} + ImVec2{width, height} / 2, text, link);

        // Draw a small point that indicates the first input (like an integrated circuits).
        const bool isLR = orientation == kLeftRight;
        device.dot(ImVec2{x, y} + (isLR ? ImVec2{dHorz, dVert} : ImVec2{width - dHorz, height - dVert}), orientation);

        // Input arrows
        for (const auto &p: inputPoints) device.arrow(p + ImVec2{isLR ? dHorz : -dHorz, 0}, 0, orientation);

        const float dx = orientation == kLeftRight ? dHorz : -dHorz;
        for (const auto &p: inputPoints) device.line(p, {p.x + dx, p.y});
        for (const auto &p: outputPoints) device.line({p.x - dx, p.y}, p);
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
    CableSchema(unsigned int n = 1) : Schema(n, n, 0, float(n) * dWire) {}

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
    InverterSchema() : BlockSchema(1, 1, 2.5f * dWire, dWire, "-1", InverterColor) {}

    void draw(Device &device) const override {
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
    void draw(Device &) const override {
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
        const float dx = (width - schema->width) / 2;
        schema->place(x + dx, y, orientation);

        const ImVec2 d = {orientation == kRightLeft ? -dx : dx, 0};
        for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = schema->inputPoint(i) - d;
        for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = schema->outputPoint(i) + d;
    }

    void draw(Device &device) const override {
        schema->draw(device);
        for (unsigned int i = 0; i < inputs; i++) device.line(inputPoint(i), schema->inputPoint(i));
        for (unsigned int i = 0; i < outputs; i++) device.line(schema->outputPoint(i), outputPoint(i));
    }

private:
    Schema *schema;
};

struct BinarySchema : Schema {
    BinarySchema(Schema *s1, Schema *s2, unsigned int inputs, unsigned int outputs, float horzGap, float width, float height)
        : Schema(inputs, outputs, width, height), schema1(s1), schema2(s2), horzGap(horzGap) {}
    BinarySchema(Schema *s1, Schema *s2, unsigned int inputs, unsigned int outputs, float horzGap)
        : BinarySchema(s1, s2, inputs, outputs, horzGap, s1->width + s2->width + horzGap, max(s1->height, s2->height)) {}
    BinarySchema(Schema *s1, Schema *s2, float horzGap) : BinarySchema(s1, s2, s1->inputs, s2->outputs, horzGap) {}
    BinarySchema(Schema *s1, Schema *s2) : BinarySchema(s1, s2, s1->inputs, s2->outputs, horizontalGap(s1, s2)) {}

    ImVec2 inputPoint(unsigned int i) const override { return schema1->inputPoint(i); }
    ImVec2 outputPoint(unsigned int i) const override { return schema2->outputPoint(i); }

    // Place the two components horizontally, centered, with enough space for the connections.
    void placeImpl() override {
        const bool isLR = orientation == kLeftRight;
        auto *leftSchema = isLR ? schema1 : schema2;
        auto *rightSchema = isLR ? schema2 : schema1;
        const float dy1 = max(0.0f, rightSchema->height - leftSchema->height) / 2.0f;
        const float dy2 = max(0.0f, leftSchema->height - rightSchema->height) / 2.0f;
        leftSchema->place(x, y + dy1, orientation);
        rightSchema->place(x + leftSchema->width + horzGap, y + dy2, orientation);
    }

    void draw(Device &device) const override {
        schema1->draw(device);
        schema2->draw(device);
    }

    Schema *schema1, *schema2;
    float horzGap;

protected:
    static float horizontalGap(const Schema *s1, const Schema *s2) { return (s1->height + s2->height) / binarySchemaHorizontalGapRatio; }
};

struct ParallelSchema : BinarySchema {
    ParallelSchema(Schema *s1, Schema *s2)
        : BinarySchema(s1, s2, s1->inputs + s2->inputs, s1->outputs + s2->outputs, 0, s1->width, s1->height + s2->height),
          inputFrontier(s1->inputs), outputFrontier(s1->outputs) {
        fgassert(s1->width == s2->width);
    }

    void placeImpl() override {
        const bool isLR = orientation == kLeftRight;
        auto *topSchema = isLR ? schema1 : schema2;
        auto *bottomSchema = isLR ? schema2 : schema1;
        topSchema->place(x, y, orientation);
        bottomSchema->place(x, y + topSchema->height, orientation);
    }

    ImVec2 inputPoint(unsigned int i) const override { return i < inputFrontier ? schema1->inputPoint(i) : schema2->inputPoint(i - inputFrontier); }
    ImVec2 outputPoint(unsigned int i) const override { return i < outputFrontier ? schema1->outputPoint(i) : schema2->outputPoint(i - outputFrontier); }

private:
    unsigned int inputFrontier;
    unsigned int outputFrontier;
};

enum { kHorDir, kUpDir, kDownDir }; // directions of connections

// Compute the direction of a connection.
// Y-axis goes from top to bottom
static int direction(const ImVec2 &a, const ImVec2 &b) {
    if (a.y > b.y) return kUpDir;
    if (a.y < b.y) return kDownDir;
    return kHorDir;
}

struct SequentialSchema : BinarySchema {
    // The components s1 and s2 must be "compatible" (s1 : n->m and s2 : m->q).
    SequentialSchema(Schema *s1, Schema *s2) : BinarySchema(s1, s2, horizontalGap(s1, s2)) {
        fgassert(s1->outputs == s2->inputs);
    }

    void draw(Device &device) const override {
        BinarySchema::draw(device);

        // Draw the internal wires aligning the vertical segments in a symmetric way when possible.
        float dx = 0, mx = 0;
        int dir = -1;
        for (unsigned int i = 0; i < schema1->outputs; i++) {
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
            if (!sequentialConnectionZigzag || src.y == dst.y) {
                // Draw a straight, potentially diagonal cable.
                device.line(src, dst);
            } else {
                // Draw a zigzag cable by traversing half the distance between, taking a sharp turn, then turning back and finishing.
                device.line(src, {src.x + mx, src.y});
                device.line({src.x + mx, src.y}, {src.x + mx, dst.y});
                device.line({src.x + mx, dst.y}, dst);
            }
        }
    }

    // Compute the horizontal gap needed to draw the internal wires.
    // It depends on the largest group of connections that go in the same direction.
    // **Side effect: May add cables to ensure the internal connections are between the same number of outputs and inputs.**
    static float horizontalGap(Schema *a, Schema *b) {
        if (a->outputs == 0) return 0;

        const float dy1 = max(0.0f, b->height - a->height) / 2.0f;
        const float dy2 = max(0.0f, a->height - b->height) / 2.0f;
        a->place(0, dy1, kLeftRight);
        b->place(0, dy2, kLeftRight);

        int dir = kHorDir;
        int size = 0;
        int MaxGroupSize[] = {0, 0, 0}; // store the size of the largest group for each direction
        for (unsigned int i = 0; i < a->outputs; i++) {
            const auto d = direction(a->outputPoint(i), b->inputPoint(i));
            size = d == dir ? size + 1 : 1;
            dir = d;
            MaxGroupSize[dir] = max(MaxGroupSize[dir], size);
        }

        return dWire * float(max(MaxGroupSize[kUpDir], MaxGroupSize[kDownDir]));
    }
};

// Place and connect two diagrams in merge composition.
// The outputs of the first schema are merged to the inputs of the second.
struct MergeSchema : BinarySchema {
    MergeSchema(Schema *s1, Schema *s2) : BinarySchema(s1, s2) {}

    void draw(Device &device) const override {
        BinarySchema::draw(device);
        for (unsigned int i = 0; i < schema1->outputs; i++) device.line(schema1->outputPoint(i), schema2->inputPoint(i % schema2->inputs));
    }
};

// Place and connect two diagrams in split composition.
// The outputs of the first schema are distributed to the inputs of the second.
struct SplitSchema : BinarySchema {
    SplitSchema(Schema *s1, Schema *s2) : BinarySchema(s1, s2) {}

    void draw(Device &device) const override {
        BinarySchema::draw(device);
        for (unsigned int i = 0; i < schema2->inputs; i++) device.line(schema1->outputPoint(i % schema1->outputs), schema2->inputPoint(i));
    }
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

    // The two schemas are centered vertically, stacked on top of each other, with stacking order dependent on orientation.
    void placeImpl() override {
        const bool isLR = orientation == kLeftRight;
        auto *topSchema = isLR ? schema2 : schema1;
        auto *bottomSchema = isLR ? schema1 : schema2;
        topSchema->place(x + (width - topSchema->width) / 2, y, kRightLeft);
        bottomSchema->place(x + (width - bottomSchema->width) / 2, y + topSchema->height, kLeftRight);

        const ImVec2 d1 = {(width - schema1->width * (isLR ? 1.0f : -1.0f)) / 2, 0};
        for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = schema1->inputPoint(i + schema2->outputs) - d1;
        for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = schema1->outputPoint(i) + d1;
    }

    void draw(Device &device) const override {
        schema1->draw(device);
        schema2->draw(device);

        // Draw the implicit feedback delay to each schema2 input
        const float dw = orientation == kLeftRight ? dWire : -dWire;
        for (unsigned int i = 0; i < schema2->inputs; i++) drawDelaySign(device, schema1->outputPoint(i) + ImVec2{float(i) * dw, 0}, dw / 2);
        // Feedback connections to each schema2 input
        for (unsigned int i = 0; i < schema2->inputs; i++) drawFeedback(device, schema1->outputPoint(i), schema2->inputPoint(i), float(i) * dWire, outputPoint(i));
        // Non-recursive output lines
        for (unsigned int i = schema2->inputs; i < outputs; i++) device.line(schema1->outputPoint(i), outputPoint(i));
        // Input lines
        for (unsigned int i = 0; i < inputs; i++) device.line(inputPoint(i), schema1->inputPoint(i + schema2->outputs));
        // Feedfront connections from each schema2 output
        for (unsigned int i = 0; i < schema2->outputs; i++) drawFeedfront(device, schema2->outputPoint(i), schema1->inputPoint(i), float(i) * dWire);
    }

private:
    // Draw a feedback connection between two points with a horizontal displacement `dx`.
    void drawFeedback(Device &device, const ImVec2 &src, const ImVec2 &dst, float dx, const ImVec2 &out) const {
        const float ox = src.x + (orientation == kLeftRight ? dx : -dx);
        const float ct = (orientation == kLeftRight ? dWire : -dWire) / 2.0f;
        const ImVec2 up(ox, src.y - ct);
        const ImVec2 br(ox + ct / 2.0f, src.y);

        device.line(up, {ox, dst.y});
        device.line({ox, dst.y}, dst);
        device.line(src, br);
        device.line(br, out);
    }

    // Draw a feedfrom connection between two points with a horizontal displacement `dx`.
    void drawFeedfront(Device &device, const ImVec2 &src, const ImVec2 &dst, float dx) const {
        const float ox = src.x + (orientation == kLeftRight ? -dx : dx);
        device.line({src.x, src.y}, {ox, src.y});
        device.line({ox, src.y}, {ox, dst.y});
        device.line({ox, dst.y}, {dst.x, dst.y});
    }

    // Draw the delay sign of a feedback connection (three sides of a square)
    static void drawDelaySign(Device &device, const ImVec2 &pos, float size) {
        const float halfSize = size / 2;
        device.line(pos - ImVec2{halfSize, 0}, pos - ImVec2{halfSize, size});
        device.line(pos - ImVec2{halfSize, size}, pos + ImVec2{halfSize, -size});
        device.line(pos + ImVec2{halfSize, -size}, pos + ImVec2{halfSize, 0});
    }

    Schema *schema1, *schema2;
};

// A TopSchema is a schema surrounded by a dashed rectangle with a label on the top left.
// The rectangle is placed at half the margin parameter.
// Arrows are added to all the outputs.
struct TopSchema : Schema {
    // A TopSchema is a schema surrounded by a dashed rectangle with a label on the top left, and arrows added to the outputs.
    TopSchema(Schema *s, string link, float margin = 10)
        : Schema(0, 0, s->width + 2 * margin, s->height + 2 * margin), schema(s), link(std::move(link)), margin(margin) {}

    void placeImpl() override { schema->place(x + margin, y + margin, orientation); }

    // Top schema has no input or output
    ImVec2 inputPoint(unsigned int) const override { throw std::runtime_error("ERROR : TopSchema::inputPoint"); }
    ImVec2 outputPoint(unsigned int) const override { throw std::runtime_error("ERROR : TopSchema::outputPoint"); }

    void draw(Device &device) const override {
        device.rect({x, y, width - 1, height - 1}, "#ffffff", link);
        schema->draw(device);
        for (unsigned int i = 0; i < schema->outputs; i++) device.arrow(schema->outputPoint(i), 0, orientation);
    }

private:
    Schema *schema;
    string link;
    float margin;
};

// A `DecorateSchema` is a schema surrounded by a dashed rectangle with a label on the top left.
struct DecorateSchema : IOSchema {
    DecorateSchema(Schema *s, string text, float margin = 10)
        : IOSchema(s->inputs, s->outputs, s->width + 2 * margin, s->height + 2 * margin), schema(s), margin(margin), text(std::move(text)) {}

    void placeImpl() override {
        schema->place(x + margin, y + margin, orientation);

        const float m = orientation == kRightLeft ? -margin : margin;
        for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = schema->inputPoint(i) - ImVec2{m, 0};
        for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = schema->outputPoint(i) + ImVec2{m, 0};
    }

    void draw(Device &device) const override {
        schema->draw(device);

        // Surrounding frame
        const auto topLeft = ImVec2{x, y} + ImVec2{margin, margin} / 2;
        const auto topRight = topLeft + ImVec2{width - margin, 0};
        const auto bottomLeft = topLeft + ImVec2{0, height - margin};
        const auto bottomRight = bottomLeft + ImVec2{width - margin, 0};
        const float textLeft = x + margin;

        device.dasharray(topLeft, bottomLeft); // left line
        device.dasharray(bottomLeft, bottomRight); // bottom line
        device.dasharray(bottomRight, topRight); // right line
        device.dasharray(topLeft, {textLeft, topLeft.y}); // top segment before text
        device.dasharray({min(textLeft + float(2 + text.size()) * dLetter * 0.75f, bottomRight.x), topLeft.y}, {bottomRight.x, topLeft.y}); // top segment after text

        device.label({textLeft, topLeft.y}, text);
        for (unsigned int i = 0; i < inputs; i++) device.line(inputPoint(i), schema->inputPoint(i));
        for (unsigned int i = 0; i < outputs; i++) device.line(schema->outputPoint(i), outputPoint(i));
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

    void draw(Device &device) const override {
        const float dx = orientation == kLeftRight ? dHorz : -dHorz;
        for (const auto &p: inputPoints) device.line(p, p + ImVec2{dx, 0});
        for (const auto &p: outputPoints) device.line(p - ImVec2{dx, 0}, p);
    }
};

// A simple rectangular box with a text and inputs and outputs.
struct RouteSchema : IOSchema {
    // Build a simple colored `RouteSchema` with a certain number of inputs and outputs, a text to be displayed, and an optional link.
    // The length of the text as well as the number of inputs and outputs are used to compute the size of the `RouteSchema`
    RouteSchema(unsigned int inputs, unsigned int outputs, float width, float height, std::vector<int> routes)
        : IOSchema(inputs, outputs, width, height), color("#EEEEAA"), routes(std::move(routes)) {}

    void draw(Device &device) const override {
        if (drawRouteFrame) {
            device.rect(ImVec4{x, y, width, height} + ImVec4{dHorz, dVert, -2 * dHorz, -2 * dVert}, color, link);
            // device.text(x + width / 2, y + height / 2, text, link);

            // Draw the orientation mark, a small point that indicates the first input (like integrated circuits).
            const bool isLR = orientation == kLeftRight;
            device.dot(ImVec2{x, y} + (isLR ? ImVec2{dHorz, dVert} : ImVec2{width - dHorz, height - dVert}), orientation);

            // Input arrows
            for (const auto &p: inputPoints) device.arrow(p + ImVec2{isLR ? dHorz : -dHorz, 0}, 0, orientation);
        }

        const float dx = orientation == kLeftRight ? dHorz : -dHorz;
        // Input/output wires
        for (const auto &p: inputPoints) device.line(p, p + ImVec2{dx, 0});
        for (const auto &p: outputPoints) device.line(p - ImVec2{dx, 0}, p);

        // Route wires
        for (unsigned int i = 0; i < routes.size() - 1; i += 2) {
            device.line(inputPoints[routes[i] - 1] + ImVec2{dx, 0}, outputPoints[routes[i + 1] - 1] - ImVec2{dx, 0});
        }
    }

protected:
    const string text, color, link;
    const std::vector<int> routes;  // Route description: s1,d2,s2,d2,...
};

Schema *makeBlockSchema(unsigned int inputs, unsigned int outputs, const string &text, const string &color, const string &link = "") {
    const float minimal = 3 * dWire;
    const float w = 2 * dHorz + max(minimal, dLetter * quantize(int(text.size())));
    const float h = 2 * dVert + max(minimal, float(max(inputs, outputs)) * dWire);
    return new BlockSchema(inputs, outputs, w, h, text, color, link);
}
Schema *makeEnlargedSchema(Schema *s, float width) { return width > s->width ? new EnlargedSchema(s, width) : s; }
Schema *makeParallelSchema(Schema *s1, Schema *s2) { return new ParallelSchema(makeEnlargedSchema(s1, s2->width), makeEnlargedSchema(s2, s1->width)); }
Schema *makeSequentialSchema(Schema *s1, Schema *s2) {
    const unsigned int o = s1->outputs;
    const unsigned int i = s2->inputs;
    auto *a = o < i ? makeParallelSchema(s1, new CableSchema(i - o)) : s1;
    auto *b = o > i ? makeParallelSchema(s2, new CableSchema(o - i)) : s2;

    return new SequentialSchema(a, b);
}
Schema *makeMergeSchema(Schema *s1, Schema *s2) { return new MergeSchema(makeEnlargedSchema(s1, dWire), makeEnlargedSchema(s2, dWire)); }
Schema *makeSplitSchema(Schema *s1, Schema *s2) { return new SplitSchema(makeEnlargedSchema(s1, dWire), makeEnlargedSchema(s2, dWire)); }

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
static int boxComplexity(Box box) {
    Tree prop = box->getProperty(dc->boxComplexityMemo);
    if (prop) return tree2int(prop);

    int v = computeComplexity(box);
    box->setProperty(dc->boxComplexityMemo, tree(v));
    return v;
}

static bool isBoxBinary(Tree t, Tree &x, Tree &y) {
    return isBoxPar(t, x, y) || isBoxSeq(t, x, y) || isBoxSplit(t, x, y) || isBoxMerge(t, x, y) || isBoxRec(t, x, y);
}

// Compute the complexity of a box expression tree according to the complexity of its subexpressions.
// Basically, it counts the number of boxes to be drawn.
// If the box-diagram expression is not evaluated, it will throw an error.
static int computeComplexity(Box box) {
    if (isBoxCut(box) || isBoxWire(box)) return 0;

    const auto *xt = getUserData(box);

    // simple elements / slot
    if (xt || isBoxInt(box) || isBoxReal(box) ||
        isBoxPrim0(box) || isBoxPrim1(box) || isBoxPrim2(box) || isBoxPrim3(box) || isBoxPrim4(box) || isBoxPrim5(box) ||
        isBoxWaveform(box) || isBoxSlot(box))
        return 1;

    // foreign elements
    if (isBoxFFun(box) || isBoxFConst(box) || isBoxFVar(box)) return 1;

    Tree t1, t2;
    // symbolic boxes
    if (isBoxSymbolic(box, t1, t2)) return 1 + boxComplexity(t2);
    // binary operators
    if (isBoxBinary(box, t1, t2)) return boxComplexity(t1) + boxComplexity(t2);

    // user interface widgets
    if (isBoxButton(box) || isBoxCheckbox(box) || isBoxVSlider(box) || isBoxHSlider(box) ||
        isBoxHBargraph(box) || isBoxVBargraph(box) || isBoxSoundfile(box) || isBoxNumEntry(box))
        return 1;

    // user interface groups
    Tree label;
    if (isBoxVGroup(box, label, t1) || isBoxHGroup(box, label, t1) || isBoxTGroup(box, label, t1) || isBoxMetadata(box, t1, t2))
        return boxComplexity(t1);

    // environment/route
    Tree t3;
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
    return makeBlockSchema(1, 0, tree2str(id), SlotColor);
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
    if (getUserData(t) != nullptr) return makeBlockSchema(xtendedArity(t), 1, xtendedName(t), NormalColor);
    if (isInverter(t)) return new InverterSchema();

    int i;
    double r;
    if (isBoxInt(t, &i) || isBoxReal(t, &r)) {
        stringstream s;
        if (isBoxInt(t)) s << i;
        else s << r;
        return makeBlockSchema(0, 1, s.str(), NumberColor);
    }

    if (isBoxWaveform(t)) return makeBlockSchema(0, 2, "waveform{...}", NormalColor);
    if (isBoxWire(t)) return new CableSchema();
    if (isBoxCut(t)) return new CutSchema();

    prim0 p0;
    prim1 p1;
    prim2 p2;
    prim3 p3;
    prim4 p4;
    prim5 p5;
    if (isBoxPrim0(t, &p0)) return makeBlockSchema(0, 1, prim0name(p0), NormalColor);
    if (isBoxPrim1(t, &p1)) return makeBlockSchema(1, 1, prim1name(p1), NormalColor);
    if (isBoxPrim2(t, &p2)) return makeBlockSchema(2, 1, prim2name(p2), NormalColor);
    if (isBoxPrim3(t, &p3)) return makeBlockSchema(3, 1, prim3name(p3), NormalColor);
    if (isBoxPrim4(t, &p4)) return makeBlockSchema(4, 1, prim4name(p4), NormalColor);
    if (isBoxPrim5(t, &p5)) return makeBlockSchema(5, 1, prim5name(p5), NormalColor);

    Tree ff;
    if (isBoxFFun(t, ff)) return makeBlockSchema(ffarity(ff), 1, ffname(ff), NormalColor);

    Tree label, chan, type, name, file;
    if (isBoxFConst(t, type, name, file) || isBoxFVar(t, type, name, file)) return makeBlockSchema(0, 1, tree2str(name), NormalColor);
    if (isBoxButton(t) || isBoxCheckbox(t) || isBoxVSlider(t) || isBoxHSlider(t) || isBoxNumEntry(t)) return makeBlockSchema(0, 1, userInterfaceDescription(t), UiColor);
    if (isBoxVBargraph(t) || isBoxHBargraph(t)) return makeBlockSchema(1, 1, userInterfaceDescription(t), UiColor);
    if (isBoxSoundfile(t, label, chan)) return makeBlockSchema(2, 2 + tree2int(chan), userInterfaceDescription(t), UiColor);

    Tree a, b;
    if (isBoxMetadata(t, a, b)) return createSchema(a);

    const bool isVGroup = isBoxVGroup(t, label, a);
    const bool isHGroup = isBoxHGroup(t, label, a);
    const bool isTGroup = isBoxTGroup(t, label, a);
    if (isVGroup || isHGroup || isTGroup) {
        const string groupId = isVGroup ? "v" : isHGroup ? "h" : "t";
        return new DecorateSchema(createSchema(a), groupId + "group(" + extractName(label) + ")");
    }
    if (isBoxSeq(t, a, b)) return makeSequentialSchema(createSchema(a), createSchema(b));
    if (isBoxPar(t, a, b)) return makeParallelSchema(createSchema(a), createSchema(b));
    if (isBoxSplit(t, a, b)) return makeSplitSchema(createSchema(a), createSchema(b));
    if (isBoxMerge(t, a, b)) return makeMergeSchema(createSchema(a), createSchema(b));
    if (isBoxRec(t, a, b)) return makeRecSchema(createSchema(a), createSchema(b));
    if (isBoxSlot(t, &i)) {
        Tree id;
        getDefNameProperty(t, id);
        return makeBlockSchema(0, 1, tree2str(id), SlotColor);
    }
    if (isBoxSymbolic(t, a, b)) {
        auto *inputSlotSchema = generateInputSlotSchema(a);
        auto *abstractionSchema = generateAbstractionSchema(inputSlotSchema, b);

        Tree id;
        if (getDefNameProperty(t, id)) return abstractionSchema;
        return new DecorateSchema(abstractionSchema, "Abstraction");
    }
    if (isBoxEnvironment(t)) return makeBlockSchema(0, 0, "environment{...}", NormalColor);

    Tree c;
    if (isBoxRoute(t, a, b, c)) {
        int ins, outs;
        vector<int> route;
        if (isBoxInt(a, &ins) && isBoxInt(b, &outs) && isIntTree(c, route)) return makeRouteSchema(ins, outs, route);

        throw std::runtime_error((stringstream("ERROR in file ") << __FILE__ << ':' << __LINE__ << ", invalid route expression : " << boxpp(t)).str());
    }

    throw std::runtime_error((stringstream("ERROR in generateInsideSchema, box expression not recognized: ") << boxpp(t)).str());
}

// Transform the provided tree and id into a unique, length-limited, alphanumeric file name.
// If the tree is not the (singular) process tree, append its hex address (without the '0x' prefix) to make the file name unique.
static string fileName(Tree t, const string &id) {
    if (id == "process") return id;
    return (views::take_while(id, [](char c) { return std::isalnum(c); }) | views::take(16) | to<string>)
        + format("-{:x}", reinterpret_cast<std::uintptr_t>(t));
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

// Compute the Pure Routing property.
// That is, expressions only made of cut, wires and slots.
// No labels will be displayed for pure routing expressions.
static bool isPureRouting(Tree t) {
    bool r;
    if (dc->pureRoutingPropertyMemo.get(t, r)) return r;

    Tree x, y;
    if (isBoxCut(t) || isBoxWire(t) || isInverter(t) || isBoxSlot(t) || (isBoxBinary(t, x, y) && isPureRouting(x) && isPureRouting(y))) {
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
            // todo Instead of scheduling now, check for a `link` in `Schema::draw`,
            //  and if one's there, create an SvgDevice and pass it, along with its link, to its children.
            //  Or, create a TopSchema here and have its `draw` method always create a new device.
            //  OR, check the box complexity inside the schema draw, and create a new device if complex. (would need to pass in the tree...)
            return makeBlockSchema(ins, outs, id, LinkColor, fileName(t, id) + ".svg");
        }
        // Draw a line around the object with its name.
        if (!isPureRouting(t)) return new DecorateSchema(generateInsideSchema(t), id);
    }

    return generateInsideSchema(t); // normal case
}

static Schema *addSchemaIO(int numConnections, bool in, Schema *x) {
    if (numConnections == 0) return x;

    Schema *y = nullptr;
    do {
        Schema *z = new ConnectorSchema();
        y = y != nullptr ? makeParallelSchema(y, z) : z;
    } while (--numConnections);

    return in ? makeSequentialSchema(y, x) : makeSequentialSchema(x, y);
}

void drawBox(Box box) {
    fs::remove_all(faustDiagramsPath);
    fs::create_directory(faustDiagramsPath);

    dc = std::make_unique<DrawContext>();
    dc->foldingFlag = boxComplexity(box) > foldThreshold;

    scheduleDrawing(box); // Schedule the initial drawing

    // Generate all the pending diagrams.
    // Each diagram is decorated with its definition name property and rendered to its own file.
    Tree t;
    while (pendingDrawing(t)) {
        Tree idTree;
        getDefNameProperty(t, idTree);
        const string &id = tree2str(idTree);
        dc->schemaFileName = fileName(t, id) + ".svg";

        int ins, outs;
        getBoxType(t, &ins, &outs);
        auto *ts = new TopSchema(new DecorateSchema(addSchemaIO(outs, false, addSchemaIO(ins, true, generateInsideSchema(t))), id), dc->backLink[t]);
        ts->place();
        SVGDevice device(faustDiagramsPath / dc->schemaFileName, ts->width, ts->height);
        ts->draw(device);
    }
}
