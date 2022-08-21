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

static const fs::path faustDiagramsPath = "FaustDiagrams"; // todo app property

// todo style props
static const int foldComplexity = 2; // individual complexity threshold before folding
static const bool scaledSVG = false; // Draw scaled SVG files
static const float binarySchemaHorizontalGapRatio = 4;
static const bool sequentialConnectionZigzag = true; // false allows for diagonal lines instead of zigzags instead of zigzags
static const bool drawRouteFrame = false;
static const float topSchemaMargin = 10;
static const float decorateSchemaMargin = 10;
static const float decorateSchemaLabelOffset = 5;
static const float dWire = 8; // distance between two wires
static const float dLetter = 4.3; // width of a letter todo derive using ImGui
static const float dHorz = 4;
static const float dVert = 4;

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
    virtual void dashrect(const ImVec4 &rect, const string &text) = 0; // Dashed rectangle with a label on the top left.
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

    void dashrect(const ImVec4 &rect, const string &text) override {
        const auto [x, y, w, h] = rect;
        const auto topLeft = ImVec2{x, y};
        const auto topRight = topLeft + ImVec2{w, 0};
        const auto bottomLeft = topLeft + ImVec2{0, h};
        const auto bottomRight = bottomLeft + ImVec2{w, 0};
        const float textLeft = x + decorateSchemaLabelOffset;

        dasharray(topLeft, bottomLeft); // left line
        dasharray(bottomLeft, bottomRight); // bottom line
        dasharray(bottomRight, topRight); // right line
        dasharray(topLeft, {textLeft, topLeft.y}); // top segment before text
        dasharray({min(textLeft + float(1 + text.size()) * dLetter * 0.75f, bottomRight.x), topLeft.y}, {bottomRight.x, topLeft.y}); // top segment after text

        label({textLeft, topLeft.y}, text);
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
    const unsigned int descendents = 0; // The number of boxes within this schema (recursively).
    const unsigned int inputs, outputs;
    const float width, height;

    // Fields populated in `place()`:
    float x = 0, y = 0;
    int orientation = kLeftRight;

    Schema(unsigned int descendents, unsigned int inputs, unsigned int outputs, float width, float height)
        : descendents(descendents), inputs(inputs), outputs(outputs), width(width), height(height) {}
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
    inline bool isLR() const { return orientation == kLeftRight; }

protected:
    virtual void placeImpl() = 0;
};

struct IOSchema : Schema {
    IOSchema(unsigned int descendents, unsigned int inputs, unsigned int outputs, float width, float height)
        : Schema(descendents, inputs, outputs, width, height) {
        for (unsigned int i = 0; i < inputs; i++) inputPoints.emplace_back(0, 0);
        for (unsigned int i = 0; i < outputs; i++) outputPoints.emplace_back(0, 0);
    }

    void placeImpl() override {
        const float dir = isLR() ? dWire : -dWire;
        const float yMid = y + height / 2.0f;
        for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = {isLR() ? x : x + width, yMid - dWire * float(inputs - 1) / 2.0f + float(i) * dir};
        for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = {isLR() ? x + width : x, yMid - dWire * float(outputs - 1) / 2.0f + float(i) * dir};
    }

    ImVec2 inputPoint(unsigned int i) const override { return inputPoints[i]; }
    ImVec2 outputPoint(unsigned int i) const override { return outputPoints[i]; }

    std::vector<ImVec2> inputPoints;
    std::vector<ImVec2> outputPoints;
};

// A simple rectangular box with text and inputs and outputs.
struct BlockSchema : IOSchema {
    BlockSchema(unsigned int inputs, unsigned int outputs, float width, float height, string text, string color, string link = "")
        : IOSchema(1, inputs, outputs, width, height), text(std::move(text)), color(std::move(color)), link(std::move(link)) {}

    void draw(Device &device) const override {
        device.rect(ImVec4{x, y, width, height} + ImVec4{dHorz, dVert, -2 * dHorz, -2 * dVert}, color, link);
        device.text(ImVec2{x, y} + ImVec2{width, height} / 2, text, link);

        // Draw a small point that indicates the first input (like an integrated circuits).
        device.dot(ImVec2{x, y} + (isLR() ? ImVec2{dHorz, dVert} : ImVec2{width - dHorz, height - dVert}), orientation);
        drawConnections(device);
    }

    void drawConnections(Device &device) const {
        const float dx = isLR() ? dHorz : -dHorz;
        for (const auto &p: inputPoints) device.line(p, p + ImVec2{dx, 0}); // Input lines
        for (const auto &p: outputPoints) device.line(p - ImVec2{dx, 0}, p); // Output lines
        for (const auto &p: inputPoints) device.arrow(p + ImVec2{dx, 0}, 0, orientation); // Input arrows
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
    CableSchema(unsigned int n = 1) : Schema(0, n, n, 0, float(n) * dWire) {}

    // Place the communication points vertically spaced by `dWire`.
    void placeImpl() override {
        for (unsigned int i = 0; i < inputs; i++) {
            const float dx = dWire * (float(i) + 0.5f);
            points[i] = {x, y + (isLR() ? dx : height - dx)};
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
        drawConnections(device);
    }
};

// Terminate a cable (cut box).
struct CutSchema : Schema {
    // A Cut is represented by a small black dot.
    // It has 1 input and no outputs.
    // It has a 0 width and a 1 wire height.
    CutSchema() : Schema(0, 1, 0, 0, dWire / 100.0f), point(0, 0) {}

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
    EnlargedSchema(Schema *s, float width) : IOSchema(s->descendents, s->inputs, s->outputs, width, s->height), schema(s) {}

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
        : Schema(s1->descendents + s2->descendents, inputs, outputs, width, height), schema1(s1), schema2(s2), horzGap(horzGap) {}
    BinarySchema(Schema *s1, Schema *s2, unsigned int inputs, unsigned int outputs, float horzGap)
        : BinarySchema(s1, s2, inputs, outputs, horzGap, s1->width + s2->width + horzGap, max(s1->height, s2->height)) {}
    BinarySchema(Schema *s1, Schema *s2, float horzGap) : BinarySchema(s1, s2, s1->inputs, s2->outputs, horzGap) {}
    BinarySchema(Schema *s1, Schema *s2) : BinarySchema(s1, s2, s1->inputs, s2->outputs, horizontalGap(s1, s2)) {}

    ImVec2 inputPoint(unsigned int i) const override { return schema1->inputPoint(i); }
    ImVec2 outputPoint(unsigned int i) const override { return schema2->outputPoint(i); }

    // Place the two components horizontally, centered, with enough space for the connections.
    void placeImpl() override {
        auto *leftSchema = isLR() ? schema1 : schema2;
        auto *rightSchema = isLR() ? schema2 : schema1;
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
        auto *topSchema = isLR() ? schema1 : schema2;
        auto *bottomSchema = isLR() ? schema2 : schema1;
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
                mx = isLR() ? (d == kDownDir ? horzGap : 0) : (d == kUpDir ? -horzGap : 0);
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
        : IOSchema(s1->descendents + s2->descendents, s1->inputs - s2->outputs, s1->outputs, width, s1->height + s2->height),
          schema1(s1), schema2(s2) {
        fgassert(s1->inputs >= s2->outputs);
        fgassert(s1->outputs >= s2->inputs);
        fgassert(s1->width >= s2->width);
    }

    // The two schemas are centered vertically, stacked on top of each other, with stacking order dependent on orientation.
    void placeImpl() override {
        auto *topSchema = isLR() ? schema2 : schema1;
        auto *bottomSchema = isLR() ? schema1 : schema2;
        topSchema->place(x + (width - topSchema->width) / 2, y, kRightLeft);
        bottomSchema->place(x + (width - bottomSchema->width) / 2, y + topSchema->height, kLeftRight);

        const ImVec2 d1 = {(width - schema1->width * (isLR() ? 1.0f : -1.0f)) / 2, 0};
        for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = schema1->inputPoint(i + schema2->outputs) - d1;
        for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = schema1->outputPoint(i) + d1;
    }

    void draw(Device &device) const override {
        schema1->draw(device);
        schema2->draw(device);

        // Draw the implicit feedback delay to each schema2 input
        const float dw = isLR() ? dWire : -dWire;
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
        const float ox = src.x + (isLR() ? dx : -dx);
        const float ct = (isLR() ? dWire : -dWire) / 2.0f;
        const ImVec2 up(ox, src.y - ct);
        const ImVec2 br(ox + ct / 2.0f, src.y);

        device.line(up, {ox, dst.y});
        device.line({ox, dst.y}, dst);
        device.line(src, br);
        device.line(br, out);
    }

    // Draw a feedfrom connection between two points with a horizontal displacement `dx`.
    void drawFeedfront(Device &device, const ImVec2 &src, const ImVec2 &dst, float dx) const {
        const float ox = src.x + (isLR() ? -dx : dx);
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

Schema *makeEnlargedSchema(Schema *s, float width) { return width > s->width ? new EnlargedSchema(s, width) : s; }
Schema *makeParallelSchema(Schema *s1, Schema *s2) { return new ParallelSchema(makeEnlargedSchema(s1, s2->width), makeEnlargedSchema(s2, s1->width)); }
Schema *makeSequentialSchema(Schema *s1, Schema *s2) {
    const unsigned int o = s1->outputs;
    const unsigned int i = s2->inputs;
    return new SequentialSchema(
        o < i ? makeParallelSchema(s1, new CableSchema(i - o)) : s1,
        o > i ? makeParallelSchema(s2, new CableSchema(o - i)) : s2
    );
}

// Transform the provided tree and id into a unique, length-limited, alphanumeric file name.
// If the tree is not the (singular) process tree, append its hex address (without the '0x' prefix) to make the file name unique.
static string svgFileName(Tree t, const string &id) {
    if (id == "process") return id + ".svg";
    return (views::take_while(id, [](char c) { return std::isalnum(c); }) | views::take(16) | to<string>)
        + format("-{:x}", reinterpret_cast<std::uintptr_t>(t)) + ".svg";
}

struct DrawContext {
    property<bool> pureRoutingPropertyMemo{}; // Avoid recomputing pure-routing property
    std::set<Tree> drawnExp; // Expressions drawn or scheduled so far
    std::stack<string> fileNames;
};

std::unique_ptr<DrawContext> dc;

// A `DecorateSchema` is a schema surrounded by a dashed rectangle with a label on the top left, and arrows added to the outputs.
// If `topLevel = true`, additional padding is added, along with output arrows.
struct DecorateSchema : IOSchema {
    DecorateSchema(Tree t, Schema *s, string text, string link = "", bool topLevel = false)
        : IOSchema(s->descendents, s->inputs, s->outputs,
        s->width + 2 * (decorateSchemaMargin + (topLevel ? topSchemaMargin : 0)),
        s->height + 2 * (decorateSchemaMargin + (topLevel ? topSchemaMargin : 0))),
          tree(t), schema(s), text(std::move(text)), link(std::move(link)), topLevel(topLevel) {}

    void placeImpl() override {
        const float margin = decorateSchemaMargin + (topLevel ? topSchemaMargin : 0);
        schema->place(x + margin, y + margin, orientation);

        const float m = orientation == kRightLeft ? -topSchemaMargin : topSchemaMargin;
        for (unsigned int i = 0; i < inputs; i++) inputPoints[i] = schema->inputPoint(i) - ImVec2{m, 0};
        for (unsigned int i = 0; i < outputs; i++) outputPoints[i] = schema->outputPoint(i) + ImVec2{m, 0};
    }

    void draw(Device &device) const override {
        if (topLevel) device.rect({x, y, width - 1, height - 1}, "#ffffff", link);

        schema->draw(device);
        const float topLevelMargin = topLevel ? topSchemaMargin : 0;
        const float margin = 2 * topLevelMargin + decorateSchemaMargin;
        device.dashrect({x + margin / 2, y + margin / 2, width - margin, height - margin}, text);
        for (unsigned int i = 0; i < inputs; i++) device.line(inputPoint(i), schema->inputPoint(i));
        for (unsigned int i = 0; i < outputs; i++) device.line(schema->outputPoint(i), outputPoint(i));

        if (topLevel) for (unsigned int i = 0; i < outputs; i++) device.arrow(outputPoint(i), 0, orientation);
    }

    void draw() const {
        const string &file_name = svgFileName(tree, text);
        SVGDevice device(faustDiagramsPath / file_name, width, height);
        draw(device);
    }

private:
    Tree tree;
    Schema *schema;
    string text, link;
    bool topLevel;
};

// A simple rectangular box with a text and inputs and outputs.
struct RouteSchema : IOSchema {
    // Build a simple colored `RouteSchema` with a certain number of inputs and outputs, a text to be displayed, and an optional link.
    // The length of the text as well as the number of inputs and outputs are used to compute the size of the `RouteSchema`
    RouteSchema(unsigned int inputs, unsigned int outputs, float width, float height, std::vector<int> routes)
        : IOSchema(0, inputs, outputs, width, height), color("#EEEEAA"), routes(std::move(routes)) {}

    void draw(Device &device) const override {
        if (drawRouteFrame) {
            device.rect(ImVec4{x, y, width, height} + ImVec4{dHorz, dVert, -2 * dHorz, -2 * dVert}, color, link);
            // device.text(x + width / 2, y + height / 2, text, link);

            // Draw the orientation mark, a small point that indicates the first input (like integrated circuits).
            device.dot(ImVec2{x, y} + (isLR() ? ImVec2{dHorz, dVert} : ImVec2{width - dHorz, height - dVert}), orientation);

            // Input arrows
            for (const auto &p: inputPoints) device.arrow(p + ImVec2{isLR() ? dHorz : -dHorz, 0}, 0, orientation);
        }

        const float dx = isLR() ? dHorz : -dHorz;
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

static bool isBoxBinary(Tree t, Tree &x, Tree &y) {
    return isBoxPar(t, x, y) || isBoxSeq(t, x, y) || isBoxSplit(t, x, y) || isBoxMerge(t, x, y) || isBoxRec(t, x, y);
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

// Collect the leaf numbers of tree `t` into vector `v`.
// Return true if `t` is a number or a parallel tree of numbers.
static bool isIntTree(Tree t, std::vector<int> &v) {
    int i;
    if (isBoxInt(t, &i)) {
        v.push_back(i);
        return true;
    }

    double r;
    if (isBoxReal(t, &r)) {
        v.push_back(int(r));
        return true;
    }

    Tree x, y;
    if (isBoxPar(t, x, y)) return isIntTree(x, v) && isIntTree(y, v);

    throw std::runtime_error((stringstream("Not a valid list of numbers : ") << boxpp(t)).str());
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
    if (isBoxInt(t, &i) || isBoxReal(t, &r)) return makeBlockSchema(0, 1, isBoxInt(t) ? std::to_string(i) : std::to_string(r), NumberColor);
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
        return new DecorateSchema(a, createSchema(a), groupId + "group(" + extractName(label) + ")");
    }
    if (isBoxSeq(t, a, b)) return makeSequentialSchema(createSchema(a), createSchema(b));
    if (isBoxPar(t, a, b)) return makeParallelSchema(createSchema(a), createSchema(b));
    if (isBoxSplit(t, a, b)) return new SplitSchema(makeEnlargedSchema(createSchema(a), dWire), makeEnlargedSchema(createSchema(b), dWire));
    if (isBoxMerge(t, a, b)) return new MergeSchema(makeEnlargedSchema(createSchema(a), dWire), makeEnlargedSchema(createSchema(b), dWire));
    if (isBoxRec(t, a, b)) {
        // The smaller component is enlarged to the width of the other.
        auto *s1 = createSchema(a);
        auto *s2 = createSchema(b);
        auto *s1e = makeEnlargedSchema(s1, s2->width);
        auto *s2e = makeEnlargedSchema(s2, s1->width);
        const float w = s1e->width + 2 * dWire * float(max(s2e->inputs, s2e->outputs));
        return new RecSchema(s1e, s2e, w);
    }
    if (isBoxSlot(t, &i)) {
        Tree id;
        getDefNameProperty(t, id);
        return makeBlockSchema(0, 1, tree2str(id), SlotColor);
    }
    if (isBoxSymbolic(t, a, b)) {
        auto *abstractionSchema = generateAbstractionSchema(generateInputSlotSchema(a), b);

        Tree id;
        if (getDefNameProperty(t, id)) return abstractionSchema;
        return new DecorateSchema(t, abstractionSchema, "Abstraction");
    }
    if (isBoxEnvironment(t)) return makeBlockSchema(0, 0, "environment{...}", NormalColor);

    Tree c;
    if (isBoxRoute(t, a, b, c)) {
        int ins, outs;
        vector<int> route;
        if (isBoxInt(a, &ins) && isBoxInt(b, &outs) && isIntTree(c, route)) {
            // Build n x m cable routing
            const float minimal = 3 * dWire;
            const float h = 2 * dVert + max(minimal, max(float(ins), float(outs)) * dWire);
            const float w = 2 * dHorz + max(minimal, h * 0.75f);
            return new RouteSchema(ins, outs, w, h, route);
        }

        throw std::runtime_error((stringstream("Invalid route expression : ") << boxpp(t)).str());
    }

    throw std::runtime_error((stringstream("ERROR in generateInsideSchema, box expression not recognized: ") << boxpp(t)).str());
}


// Each top-level schema is decorated with its definition name property and rendered to its own file.
static DecorateSchema makeTopLevelSchema(Tree t) {
    Tree idTree;
    getDefNameProperty(t, idTree);
    const string &id = tree2str(idTree);
    const string &file_name = svgFileName(t, id);
    const string enclosingFileName = dc->fileNames.empty() ? "" : dc->fileNames.top();
    dc->fileNames.push(file_name);
    DecorateSchema schema = {t, generateInsideSchema(t), id, enclosingFileName, true};
    dc->fileNames.pop();
    return schema;
}

// Returns `true` if the tree is only made of cut, wires and slots.
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

static Schema *createSchema(Tree t) {
    Tree idTree;
    if (getDefNameProperty(t, idTree)) {
        const string &id = tree2str(idTree);
        auto schema = makeTopLevelSchema(t);
        if (schema.descendents >= foldComplexity) {
            if (!dc->drawnExp.contains(t)) {
                dc->drawnExp.insert(t);
                schema.place();
                schema.draw();
            }
            int ins, outs;
            getBoxType(t, &ins, &outs);
            return makeBlockSchema(ins, outs, id, LinkColor, svgFileName(t, id));
        }
        // Draw a line around the object with its name.
        if (!isPureRouting(t)) return new DecorateSchema(t, generateInsideSchema(t), id);
    }

    return generateInsideSchema(t); // normal case
}

void drawBox(Box box) {
    fs::remove_all(faustDiagramsPath);
    fs::create_directory(faustDiagramsPath);

    dc = std::make_unique<DrawContext>();
    auto schema = makeTopLevelSchema(box);
    schema.place();
    schema.draw();
}
