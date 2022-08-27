#include "DrawBox.hh"

#include <sstream>
#include <map>
#include <stack>

#include "../../Context.h"

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/take_while.hpp>
#include <range/v3/numeric/accumulate.hpp>

#include "boxes/ppbox.hh"
#include "faust/dsp/libfaust-signal.h"

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
static const float inverterRadius = 1.5;

// todo move to FlowGridStyle::Colors
static const string LinkColor = "#003366";
static const string NormalColor = "#4b71a1";
static const string UiColor = "#477881";
static const string SlotColor = "#47945e";
static const string NumberColor = "#f44800";
static const string InverterColor = "#ffffff";

using Count = unsigned int;
enum Orientation { LeftRight = 1, RightLeft = -1 };
enum DeviceType { ImGuiDeviceType, SVGDeviceType };

class Device {
public:
    virtual ~Device() = default;
    virtual DeviceType type() = 0;
    virtual void rect(const ImVec4 &rect, const string &color, const string &link) = 0;
    virtual void grouprect(const ImVec4 &rect, const string &text) = 0; // A labeled grouping
    virtual void triangle(const ImVec2 &a, const ImVec2 &b, const ImVec2 &c, const string &color) = 0;
    virtual void circle(const ImVec2 &pos, float radius, const string &color) = 0;
    virtual void arrow(const ImVec2 &pos, float rotation, Orientation orientation) = 0;
    virtual void line(const ImVec2 &start, const ImVec2 &end) = 0;
    virtual void text(const ImVec2 &pos, const string &name, const string &link) = 0;
    virtual void dot(const ImVec2 &pos, Orientation orientation) = 0;
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

    DeviceType type() override { return SVGDeviceType; }

    static string xml_sanitize(const string &name) {
        static std::map<char, string> replacements{{'<', "&lt;"}, {'>', "&gt;"}, {'\'', "&apos;"}, {'"', "&quot;"}, {'&', "&amp;"}};

        auto replaced_name = name;
        for (const auto &[ch, replacement]: replacements) {
            replaced_name = replace(replaced_name, ch, replacement);
        }
        return replaced_name;
    }

    void rect(const ImVec4 &rect, const string &color, const string &link) override {
        if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link)); // open the optional link tag
        const auto [x, y, w, h] = rect;
        stream << format(R"(<rect x="{}" y="{}" width="{}" height="{}" rx="0" ry="0" style="stroke:none;fill:{};"/>)", x, y, w, h, color);
        if (!link.empty()) stream << "</a>"; // close the optional link tag
    }

    // SVG implements a group rect as a dashed rectangle with a label on the top left.
    void grouprect(const ImVec4 &rect, const string &text) override {
        const auto [x, y, w, h] = rect;
        const auto topLeft = ImVec2{x, y};
        const auto topRight = topLeft + ImVec2{w, 0};
        const auto bottomLeft = topLeft + ImVec2{0, h};
        const auto bottomRight = bottomLeft + ImVec2{w, 0};
        const float textLeft = x + decorateSchemaLabelOffset;

        stream << dash_line(topLeft, bottomLeft); // left line
        stream << dash_line(bottomLeft, bottomRight); // bottom line
        stream << dash_line(bottomRight, topRight); // right line
        stream << dash_line(topLeft, {textLeft, topLeft.y}); // top segment before text
        stream << dash_line({min(textLeft + float(1 + text.size()) * dLetter * 0.75f, bottomRight.x), topLeft.y}, {bottomRight.x, topLeft.y}); // top segment after text

        stream << label({textLeft, topLeft.y}, text);
    }

    void triangle(const ImVec2 &a, const ImVec2 &b, const ImVec2 &c, const string &color) override {
        stream << format(R"(<polygon fill="{}" stroke="black" stroke-width=".25" points="{},{} {},{} {},{}"/>)", color, a.x, a.y, b.x, b.y, c.x, c.y);
    }

    void circle(const ImVec2 &pos, float radius, const string &color) override {
        const auto [x, y] = pos;
        stream << format(R"(<circle fill="{}" stroke="black" stroke-width=".25" cx="{}" cy="{}" r="{}"/>)", color, x, y, radius);
    }

    // todo remove `rotation` arg
    void arrow(const ImVec2 &pos, float rotation, Orientation orientation) override {
        static const float dx = 3, dy = 1;
        const auto [x, y] = pos;
        const auto x1 = orientation == LeftRight ? x - dx : x + dx;
        stream << rotate_line({x1, y - dy}, pos, rotation, x, y);
        stream << rotate_line({x1, y + dy}, pos, rotation, x, y);
    }

    void line(const ImVec2 &start, const ImVec2 &end) override {
        stream << format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}"  style="stroke:black; stroke-linecap:round; stroke-width:0.25;"/>)", start.x, start.y, end.x, end.y);
    }

    void text(const ImVec2 &pos, const string &name, const string &link) override {
        if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link)); // open the optional link tag
        stream << format(R"(<text x="{}" y="{}" font-family="Arial" font-size="7" text-anchor="middle" fill="#FFFFFF">{}</text>)", pos.x, pos.y + 2, xml_sanitize(name));
        if (!link.empty()) stream << "</a>"; // close the optional link tag
    }

    void dot(const ImVec2 &pos, Orientation orientation) override {
        const float offset = orientation == LeftRight ? 2 : -2;
        stream << format(R"(<circle cx="{}" cy="{}" r="1"/>)", pos.x + offset, pos.y + offset);
    }

    static string label(const ImVec2 &pos, const string &name) {
        return format(R"(<text x="{}" y="{}" font-family="Arial" font-size="7">{}</text>)", pos.x, pos.y + 2, xml_sanitize(name));
    }

    static string dash_line(const ImVec2 &start, const ImVec2 &end) {
        return format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}"  style="stroke: black; stroke-linecap:round; stroke-width:0.25; stroke-dasharray:3,3;"/>)", start.x, start.y, end.x, end.y);
    }

    static string rotate_line(const ImVec2 &start, const ImVec2 &end, float rx, float ry, float rz) {
        return format(R"lit(<line x1="{}" y1="{}" x2="{}" y2="{}" transform="rotate({},{},{})" style="stroke: black; stroke-width:0.25;"/>)lit", start.x, start.y, end.x, end.y, rx, ry, rz);
    }

private:
    string file_name;
    std::stringstream stream;
};

static ImU32 convertColor(const string &color) {
    unsigned int x;
    std::stringstream ss;
    ss << std::hex << color;
    ss >> x;
    return x;
}

struct ImGuiDevice : Device {
    ImGuiDevice() {
        draw_list = ImGui::GetWindowDrawList();
        pos = ImGui::GetWindowPos();
    }

    ~ImGuiDevice() override = default;

    DeviceType type() override { return ImGuiDeviceType; }

    void rect(const ImVec4 &rect, const string &color, const string &link) override {
        const auto [x, y, w, h] = rect;
//        drawList->AddRectFilled({x, y}, {x + w, y + h}, convertColor(color));
        draw_list->AddRectFilled(pos + ImVec2{x, y}, pos + ImVec2{x + w, y + h}, ImGui::GetColorU32(ImGuiCol_Button));
    }

    void grouprect(const ImVec4 &rect, const string &text) override {
        const auto [x, y, w, h] = rect;
        const ImVec2 textPos = {x + decorateSchemaLabelOffset, y};
        draw_list->AddRect(pos + ImVec2{x, y}, pos + ImVec2{x + w, y + h}, ImGui::GetColorU32(ImGuiCol_Border));
        draw_list->AddText(pos + textPos, ImGui::GetColorU32(ImGuiCol_Text), text.c_str());
    }

    void triangle(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const string &color) override {
        draw_list->AddTriangle(pos + p1, pos + p2, pos + p3, ImGui::GetColorU32(ImGuiCol_Border));
    }

    void circle(const ImVec2 &p, float radius, const string &color) override {
        draw_list->AddCircle(pos + p, radius, ImGui::GetColorU32(ImGuiCol_Border));
    }

    void arrow(const ImVec2 &p, float rotation, Orientation orientation) override {
        static const ImVec2 d{6, 2};
        ImGui::RenderArrowPointingAt(draw_list, pos + p, d, orientation == LeftRight ? ImGuiDir_Right : ImGuiDir_Left, ImGui::GetColorU32(ImGuiCol_Border));
    }

    void line(const ImVec2 &start, const ImVec2 &end) override {
        draw_list->AddLine(pos + start, pos + end, ImGui::GetColorU32(ImGuiCol_Border));
    }

    void text(const ImVec2 &p, const string &name, const string &link) override {
        draw_list->AddText(pos + p, ImGui::GetColorU32(ImGuiCol_Text), name.c_str());
    }

    void dot(const ImVec2 &p, Orientation orientation) override {
        const float offset = orientation == LeftRight ? 2 : -2;
        draw_list->AddCircle(pos + p + ImVec2{offset, offset}, 1, ImGui::GetColorU32(ImGuiCol_Border));
    }

private:
    ImDrawList *draw_list;
    ImVec2 pos{};
};

static const char *getTreeName(Tree t) {
    Tree name;
    return getDefNameProperty(t, name) ? tree2str(name) : nullptr;
}

// Transform the provided tree and id into a unique, length-limited, alphanumeric file name.
// If the tree is not the (singular) process tree, append its hex address (without the '0x' prefix) to make the file name unique.
static string svgFileName(Tree t) {
    if (!t) return "";
    const string &treeName = getTreeName(t);
    if (treeName == "process") return treeName + ".svg";
    return (views::take_while(treeName, [](char c) { return std::isalnum(c); }) | views::take(16) | to<string>)
        + format("-{:x}", reinterpret_cast<std::uintptr_t>(t)) + ".svg";
}

// An abstract block diagram schema
struct Schema {
    Tree tree;
    const Count inputs, outputs;
    const float width, height;
    const std::vector<Schema *> children{};
    const Count descendents = 0; // The number of boxes within this schema (recursively).
    const bool topLevel;
    Tree parent;

    // Fields populated in `place()`:
    float x = 0, y = 0;
    Orientation orientation = LeftRight;

    Schema(Tree t, Count inputs, Count outputs, float width, float height, std::vector<Schema *> children = {}, Count directDescendents = 0, Tree parent = nullptr)
        : tree(t), inputs(inputs), outputs(outputs), width(width), height(height), children(std::move(children)),
          descendents(directDescendents + ::ranges::accumulate(this->children | views::transform([](Schema *child) { return child->descendents; }), 0)),
          topLevel(descendents >= foldComplexity), parent(parent) {}
    virtual ~Schema() = default;

    void place(float new_x, float new_y, Orientation new_orientation) {
        x = new_x;
        y = new_y;
        orientation = new_orientation;
        placeImpl();
    }
    void place() { placeImpl(); }
    void draw(Device &device) const {
        for (const auto *child: children) child->draw(device);
        drawImpl(device);
    };
    virtual ImVec2 inputPoint(Count i) const = 0;
    virtual ImVec2 outputPoint(Count i) const = 0;
    inline bool isLR() const { return orientation == LeftRight; }

    void draw(DeviceType type) const {
        if (type == SVGDeviceType) {
            SVGDevice device(faustDiagramsPath / svgFileName(tree), width, height);
            device.rect({x, y, width - 1, height - 1}, "#ffffff", svgFileName(parent));
            draw(device);
        } else {
            ImGuiDevice device;
            draw(device);
        }
    }

protected:
    virtual void placeImpl() = 0;
    virtual void drawImpl(Device &) const {};
};

struct IOSchema : Schema {
    IOSchema(Tree t, Count inputs, Count outputs, float width, float height, std::vector<Schema *> children = {}, Count directDescendents = 0, Tree parent = nullptr)
        : Schema(t, inputs, outputs, width, height, std::move(children), directDescendents, parent) {}

    void placeImpl() override {
        const float dir = isLR() ? dWire : -dWire;
        const float yMid = y + height / 2;
        for (Count i = 0; i < inputs; i++) inputPoints[i] = {isLR() ? x : x + width, yMid - dWire * float(inputs - 1) / 2 + float(i) * dir};
        for (Count i = 0; i < outputs; i++) outputPoints[i] = {isLR() ? x + width : x, yMid - dWire * float(outputs - 1) / 2 + float(i) * dir};
    }

    ImVec2 inputPoint(Count i) const override { return inputPoints[i]; }
    ImVec2 outputPoint(Count i) const override { return outputPoints[i]; }

    std::vector<ImVec2> inputPoints{inputs};
    std::vector<ImVec2> outputPoints{outputs};
};

// A simple rectangular box with text and inputs and outputs.
struct BlockSchema : IOSchema {
    BlockSchema(Tree t, Count inputs, Count outputs, float width, float height, string text, string color, Schema *schema = nullptr)
        : IOSchema(t, inputs, outputs, width, height, {}, 1, parent), text(std::move(text)), color(std::move(color)), schema(schema) {}

    void placeImpl() override {
        IOSchema::placeImpl();
        if (schema) schema->place();
    }

    void drawImpl(Device &device) const override {
        if (schema) schema->draw(device.type());
        const string &link = schema ? svgFileName(tree) : "";
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

    const string text, color;
    Schema *schema;
};

static inline float quantize(int n) {
    static const int q = 3;
    return float(q * ((n + q - 1) / q)); // NOLINT(bugprone-integer-division)
}

// Simple cables (identity box) in parallel.
// The width of a cable is null.
// Therefor, input and output connection points are the same.
struct CableSchema : Schema {
    CableSchema(Tree t, Count n = 1) : Schema(t, n, n, 0, float(n) * dWire) {}

    // Place the communication points vertically spaced by `dWire`.
    void placeImpl() override {
        for (Count i = 0; i < inputs; i++) {
            const float dx = dWire * (float(i) + 0.5f);
            points[i] = {x, y + (isLR() ? dx : height - dx)};
        }
    }

    ImVec2 inputPoint(Count i) const override { return points[i]; }
    ImVec2 outputPoint(Count i) const override { return points[i]; }

private:
    std::vector<ImVec2> points{inputs};
};

// An inverter is a special symbol corresponding to '*(-1)', used to create more compact diagrams.
struct InverterSchema : BlockSchema {
    InverterSchema(Tree t) : BlockSchema(t, 1, 1, 2.5f * dWire, dWire, "-1", InverterColor) {}

    void drawImpl(Device &device) const override {
        const ImVec2 pos = {x, y};
        const float x1 = width - 2 * dHorz;
        const float y1 = 0.5f + (height - 1) / 2;
        const auto ta = pos + ImVec2{dHorz + (isLR() ? 0 : x1), 0};
        const auto tb = ta + ImVec2{(isLR() ? x1 - 2 * inverterRadius : 2 * inverterRadius - x1 - pos.x), y1};
        const auto tc = ta + ImVec2{0, height - 1};
        const auto circlePos = tb + ImVec2{isLR() ? inverterRadius : -inverterRadius, 0};
        device.circle(circlePos, inverterRadius, color);
        device.triangle(ta, tb, tc, color);
        drawConnections(device);
    }
};

// Terminate a cable (cut box).
struct CutSchema : Schema {
    // A Cut is represented by a small black dot.
    // It has 1 input and no outputs.
    // It has a 0 width and a 1 wire height.
    CutSchema(Tree t) : Schema(t, 1, 0, 0, dWire / 100.0f), point(0, 0) {}

    // The input point is placed in the middle.
    void placeImpl() override { point = {x, y + height * 0.5f}; }

    // A cut is represented by a small black dot.
    void drawImpl(Device &) const override {
        //    device.circle(point, dWire / 8.0);
    }

    // By definition, a Cut has only one input point.
    ImVec2 inputPoint(Count) const override { return point; }

    // By definition, a Cut has no output point.
    ImVec2 outputPoint(Count) const override {
        fgassert(false);
        return {-1, -1};
    }

private:
    ImVec2 point;
};

struct EnlargedSchema : IOSchema {
    EnlargedSchema(Schema *s, float width) : IOSchema(s->tree, s->inputs, s->outputs, width, s->height, {s}) {}

    void placeImpl() override {
        auto *schema = children[0];
        const float dx = (width - schema->width) / 2;
        schema->place(x + dx, y, orientation);

        const ImVec2 d = {isLR() ? dx : -dx, 0};
        for (Count i = 0; i < inputs; i++) inputPoints[i] = schema->inputPoint(i) - d;
        for (Count i = 0; i < outputs; i++) outputPoints[i] = schema->outputPoint(i) + d;
    }

    void drawImpl(Device &device) const override {
        const auto &schema = children[0];
        for (Count i = 0; i < inputs; i++) device.line(inputPoint(i), schema->inputPoint(i));
        for (Count i = 0; i < outputs; i++) device.line(schema->outputPoint(i), outputPoint(i));
    }
};

struct BinarySchema : Schema {
    BinarySchema(Tree t, Schema *s1, Schema *s2, Count inputs, Count outputs, float horzGap, float width, float height)
        : Schema(t, inputs, outputs, width, height, {s1, s2}), horzGap(horzGap) {}
    BinarySchema(Tree t, Schema *s1, Schema *s2, Count inputs, Count outputs, float horzGap)
        : BinarySchema(t, s1, s2, inputs, outputs, horzGap, s1->width + s2->width + horzGap, max(s1->height, s2->height)) {}
    BinarySchema(Tree t, Schema *s1, Schema *s2, float horzGap) : BinarySchema(t, s1, s2, s1->inputs, s2->outputs, horzGap) {}
    BinarySchema(Tree t, Schema *s1, Schema *s2) : BinarySchema(t, s1, s2, s1->inputs, s2->outputs, horizontalGap(s1, s2)) {}

    ImVec2 inputPoint(Count i) const override { return children[0]->inputPoint(i); }
    ImVec2 outputPoint(Count i) const override { return children[1]->outputPoint(i); }

    // Place the two components horizontally, centered, with enough space for the connections.
    void placeImpl() override {
        auto *leftSchema = children[isLR() ? 0 : 1];
        auto *rightSchema = children[isLR() ? 1 : 0];
        const float dy1 = max(0.0f, rightSchema->height - leftSchema->height) / 2;
        const float dy2 = max(0.0f, leftSchema->height - rightSchema->height) / 2;
        leftSchema->place(x, y + dy1, orientation);
        rightSchema->place(x + leftSchema->width + horzGap, y + dy2, orientation);
    }

    float horzGap;

protected:
    static float horizontalGap(const Schema *s1, const Schema *s2) { return (s1->height + s2->height) / binarySchemaHorizontalGapRatio; }
};

struct ParallelSchema : BinarySchema {
    ParallelSchema(Tree t, Schema *s1, Schema *s2)
        : BinarySchema(t, s1, s2, s1->inputs + s2->inputs, s1->outputs + s2->outputs, 0, s1->width, s1->height + s2->height),
          inputFrontier(s1->inputs), outputFrontier(s1->outputs) {
        fgassert(s1->width == s2->width);
    }

    void placeImpl() override {
        auto *topSchema = children[isLR() ? 0 : 1];
        auto *bottomSchema = children[isLR() ? 1 : 0];
        topSchema->place(x, y, orientation);
        bottomSchema->place(x, y + topSchema->height, orientation);
    }

    ImVec2 inputPoint(Count i) const override { return i < inputFrontier ? children[0]->inputPoint(i) : children[1]->inputPoint(i - inputFrontier); }
    ImVec2 outputPoint(Count i) const override { return i < outputFrontier ? children[0]->outputPoint(i) : children[1]->outputPoint(i - outputFrontier); }

private:
    Count inputFrontier, outputFrontier;
};

struct SequentialSchema : BinarySchema {
    // The components s1 and s2 must be "compatible" (s1: n->m and s2: m->q).
    SequentialSchema(Tree t, Schema *s1, Schema *s2) : BinarySchema(t, s1, s2, horizontalGap(s1, s2)) {
        fgassert(s1->outputs == s2->inputs);
    }

    // Compute the direction of a connection.
    // Y-axis goes from top to bottom
    static ImGuiDir connectionDirection(const ImVec2 &a, const ImVec2 &b) { return a.y > b.y ? ImGuiDir_Up : (a.y < b.y ? ImGuiDir_Down : ImGuiDir_Right); }

    void drawImpl(Device &device) const override {
        BinarySchema::drawImpl(device);

        // Draw the internal wires aligning the vertical segments in a symmetric way when possible.
        float dx = 0, mx = 0;
        ImGuiDir direction = ImGuiDir_None;
        for (Count i = 0; i < children[0]->outputs; i++) {
            const auto src = children[0]->outputPoint(i);
            const auto dst = children[1]->inputPoint(i);
            const ImGuiDir d = connectionDirection(src, dst);
            if (d == direction) {
                mx += dx; // move in same direction
            } else {
                mx = isLR() ? (d == ImGuiDir_Down ? horzGap : 0) : (d == ImGuiDir_Up ? -horzGap : 0);
                dx = d == ImGuiDir_Up ? dWire : d == ImGuiDir_Down ? -dWire : 0;
                direction = d;
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

        const float dy1 = max(0.0f, b->height - a->height) / 2;
        const float dy2 = max(0.0f, a->height - b->height) / 2;
        a->place(0, dy1, LeftRight);
        b->place(0, dy2, LeftRight);

        ImGuiDir direction = ImGuiDir_None;
        Count size = 0;
        std::map<ImGuiDir, Count> MaxGroupSize; // store the size of the largest group for each direction
        for (Count i = 0; i < a->outputs; i++) {
            const auto d = connectionDirection(a->outputPoint(i), b->inputPoint(i));
            size = (d == direction ? size + 1 : 1);
            direction = d;
            MaxGroupSize[direction] = max(MaxGroupSize[direction], size);
        }

        return dWire * float(max(MaxGroupSize[ImGuiDir_Up], MaxGroupSize[ImGuiDir_Down]));
    }
};

// Place and connect two diagrams in merge composition.
// The outputs of the first schema are merged to the inputs of the second.
struct MergeSchema : BinarySchema {
    MergeSchema(Tree t, Schema *s1, Schema *s2) : BinarySchema(t, s1, s2) {}

    void drawImpl(Device &device) const override {
        BinarySchema::drawImpl(device);
        for (Count i = 0; i < children[0]->outputs; i++) device.line(children[0]->outputPoint(i), children[1]->inputPoint(i % children[1]->inputs));
    }
};

// Place and connect two diagrams in split composition.
// The outputs of the first schema are distributed to the inputs of the second.
struct SplitSchema : BinarySchema {
    SplitSchema(Tree t, Schema *s1, Schema *s2) : BinarySchema(t, s1, s2) {}

    void drawImpl(Device &device) const override {
        BinarySchema::drawImpl(device);
        for (Count i = 0; i < children[1]->inputs; i++) device.line(children[0]->outputPoint(i % children[0]->outputs), children[1]->inputPoint(i));
    }
};

// Place and connect two diagrams in recursive composition
// The two components must have the same width.
struct RecursiveSchema : IOSchema {
    RecursiveSchema(Tree t, Schema *s1, Schema *s2, float width)
        : IOSchema(t, s1->inputs - s2->outputs, s1->outputs, width, s1->height + s2->height, {s1, s2}) {
        fgassert(s1->inputs >= s2->outputs);
        fgassert(s1->outputs >= s2->inputs);
        fgassert(s1->width >= s2->width);
    }

    // The two schemas are centered vertically, stacked on top of each other, with stacking order dependent on orientation.
    void placeImpl() override {
        auto *topSchema = children[isLR() ? 1 : 0];
        auto *bottomSchema = children[isLR() ? 0 : 1];
        topSchema->place(x + (width - topSchema->width) / 2, y, RightLeft);
        bottomSchema->place(x + (width - bottomSchema->width) / 2, y + topSchema->height, LeftRight);

        const ImVec2 d1 = {(width - children[0]->width * (isLR() ? 1.0f : -1.0f)) / 2, 0};
        for (Count i = 0; i < inputs; i++) inputPoints[i] = children[0]->inputPoint(i + children[1]->outputs) - d1;
        for (Count i = 0; i < outputs; i++) outputPoints[i] = children[0]->outputPoint(i) + d1;
    }

    void drawImpl(Device &device) const override {
        const auto *s1 = children[0];
        const auto *s2 = children[1];
        // Draw the implicit feedback delay to each schema2 input
        const float dw = isLR() ? dWire : -dWire;
        for (Count i = 0; i < s2->inputs; i++) drawDelaySign(device, s1->outputPoint(i) + ImVec2{float(i) * dw, 0}, dw / 2);
        // Feedback connections to each schema2 input
        for (Count i = 0; i < s2->inputs; i++) drawFeedback(device, s1->outputPoint(i), s2->inputPoint(i), float(i) * dWire, outputPoint(i));
        // Non-recursive output lines
        for (Count i = s2->inputs; i < outputs; i++) device.line(s1->outputPoint(i), outputPoint(i));
        // Input lines
        for (Count i = 0; i < inputs; i++) device.line(inputPoint(i), s1->inputPoint(i + s2->outputs));
        // Feedfront connections from each schema2 output
        for (Count i = 0; i < s2->outputs; i++) drawFeedfront(device, s2->outputPoint(i), s1->inputPoint(i), float(i) * dWire);
    }

private:
    // Draw a feedback connection between two points with a horizontal displacement `dx`.
    void drawFeedback(Device &device, const ImVec2 &src, const ImVec2 &dst, float dx, const ImVec2 &out) const {
        const float ox = src.x + (isLR() ? dx : -dx);
        const float ct = (isLR() ? dWire : -dWire) / 2;
        const ImVec2 up(ox, src.y - ct);
        const ImVec2 br(ox + ct / 2, src.y);

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
};

Schema *makeEnlargedSchema(Schema *s, float width) { return width > s->width ? new EnlargedSchema(s, width) : s; }
Schema *makeParallelSchema(Tree t, Schema *s1, Schema *s2) { return new ParallelSchema(t, makeEnlargedSchema(s1, s2->width), makeEnlargedSchema(s2, s1->width)); }
Schema *makeSequentialSchema(Tree t, Schema *s1, Schema *s2) {
    const auto o = s1->outputs;
    const auto i = s2->inputs;
    return new SequentialSchema(t,
        o < i ? makeParallelSchema(t, s1, new CableSchema(t, i - o)) : s1,
        o > i ? makeParallelSchema(t, s2, new CableSchema(t, o - i)) : s2
    );
}

struct DrawContext {
    std::map<Tree, bool> isTreePureRouting{}; // Avoid recomputing pure-routing property
    std::stack<Tree> treeFocusHierarchy; // As we descend into the tree, keep track of what the ordered set of ancestors for backlinks.
};

std::unique_ptr<DrawContext> dc;

// A `DecorateSchema` is a schema surrounded by a dashed rectangle with a label on the top left, and arrows added to the outputs.
// If `topLevel = true`, additional padding is added, along with output arrows.
struct DecorateSchema : IOSchema {
    DecorateSchema(Tree t, Schema *s, string text, Tree parent = nullptr)
        : IOSchema(t, s->inputs, s->outputs,
        s->width + 2 * (decorateSchemaMargin + (s->topLevel ? topSchemaMargin : 0)),
        s->height + 2 * (decorateSchemaMargin + (s->topLevel ? topSchemaMargin : 0)),
        {s}, 0, parent), text(std::move(text)) {}

    void placeImpl() override {
        const float margin = decorateSchemaMargin + (topLevel ? topSchemaMargin : 0);
        const auto &schema = children[0];
        schema->place(x + margin, y + margin, orientation);

        const ImVec2 m = {isLR() ? topSchemaMargin : -topSchemaMargin, 0};
        for (Count i = 0; i < inputs; i++) inputPoints[i] = schema->inputPoint(i) - m;
        for (Count i = 0; i < outputs; i++) outputPoints[i] = schema->outputPoint(i) + m;
    }

    void drawImpl(Device &device) const override {
        const auto &schema = children[0];
        const float topLevelMargin = topLevel ? topSchemaMargin : 0;
        const float margin = 2 * topLevelMargin + decorateSchemaMargin;
        device.grouprect({x + margin / 2, y + margin / 2, width - margin, height - margin}, text);
        for (Count i = 0; i < inputs; i++) device.line(inputPoint(i), schema->inputPoint(i));
        for (Count i = 0; i < outputs; i++) device.line(schema->outputPoint(i), outputPoint(i));

        if (topLevel) for (Count i = 0; i < outputs; i++) device.arrow(outputPoint(i), 0, orientation);
    }

private:
    string text;
};

struct RouteSchema : IOSchema {
    RouteSchema(Tree t, Count inputs, Count outputs, float width, float height, std::vector<int> routes)
        : IOSchema(t, inputs, outputs, width, height), color("#EEEEAA"), routes(std::move(routes)) {}

    void drawImpl(Device &device) const override {
        if (drawRouteFrame) {
            device.rect(ImVec4{x, y, width, height} + ImVec4{dHorz, dVert, -2 * dHorz, -2 * dVert}, color, "");
            // Draw the orientation mark, a small point that indicates the first input (like integrated circuits).
            device.dot(ImVec2{x, y} + (isLR() ? ImVec2{dHorz, dVert} : ImVec2{width - dHorz, height - dVert}), orientation);
            // Input arrows
            for (const auto &p: inputPoints) device.arrow(p + ImVec2{isLR() ? dHorz : -dHorz, 0}, 0, orientation);
        }

        // Input/output & route wires
        const auto d = ImVec2{isLR() ? dHorz : -dHorz, 0};
        for (const auto &p: inputPoints) device.line(p, p + d);
        for (const auto &p: outputPoints) device.line(p - d, p);
        for (Count i = 0; i < routes.size() - 1; i += 2) device.line(inputPoints[routes[i] - 1] + d, outputPoints[routes[i + 1] - 1] - d);
    }

protected:
    const string color;
    const std::vector<int> routes;  // Route description: s1,d2,s2,d2,...
};

Schema *makeBlockSchema(Tree t, Count inputs, Count outputs, const string &text, const string &color, Schema *innerSchema = nullptr) {
    const float minimal = 3 * dWire;
    const float w = 2 * dHorz + max(minimal, dLetter * quantize(int(text.size())));
    const float h = 2 * dVert + max(minimal, float(max(inputs, outputs)) * dWire);
    return new BlockSchema(t, inputs, outputs, w, h, text, color, innerSchema);
}

static bool isBoxBinary(Tree t, Tree &x, Tree &y) {
    return isBoxPar(t, x, y) || isBoxSeq(t, x, y) || isBoxSplit(t, x, y) || isBoxMerge(t, x, y) || isBoxRec(t, x, y);
}

static Schema *createSchema(Tree t, bool no_block = false);

// Generate a 1->0 block schema for an input slot.
static Schema *generateInputSlotSchema(Tree t) { return makeBlockSchema(t, 1, 0, getTreeName(t), SlotColor); }

// Generate an abstraction schema by placing in sequence the input slots and the body.
static Schema *generateAbstractionSchema(Schema *x, Tree t) {
    Tree a, b;
    while (isBoxSymbolic(t, a, b)) {
        x = makeParallelSchema(t, x, generateInputSlotSchema(a));
        t = b;
    }
    return makeSequentialSchema(t, x, createSchema(t));
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
    if (getUserData(t) != nullptr) return makeBlockSchema(t, xtendedArity(t), 1, xtendedName(t), NormalColor);
    if (isInverter(t)) return new InverterSchema(t);

    int i;
    double r;
    if (isBoxInt(t, &i) || isBoxReal(t, &r)) return makeBlockSchema(t, 0, 1, isBoxInt(t) ? std::to_string(i) : std::to_string(r), NumberColor);
    if (isBoxWaveform(t)) return makeBlockSchema(t, 0, 2, "waveform{...}", NormalColor);
    if (isBoxWire(t)) return new CableSchema(t);
    if (isBoxCut(t)) return new CutSchema(t);

    prim0 p0;
    prim1 p1;
    prim2 p2;
    prim3 p3;
    prim4 p4;
    prim5 p5;
    if (isBoxPrim0(t, &p0)) return makeBlockSchema(t, 0, 1, prim0name(p0), NormalColor);
    if (isBoxPrim1(t, &p1)) return makeBlockSchema(t, 1, 1, prim1name(p1), NormalColor);
    if (isBoxPrim2(t, &p2)) return makeBlockSchema(t, 2, 1, prim2name(p2), NormalColor);
    if (isBoxPrim3(t, &p3)) return makeBlockSchema(t, 3, 1, prim3name(p3), NormalColor);
    if (isBoxPrim4(t, &p4)) return makeBlockSchema(t, 4, 1, prim4name(p4), NormalColor);
    if (isBoxPrim5(t, &p5)) return makeBlockSchema(t, 5, 1, prim5name(p5), NormalColor);

    Tree ff;
    if (isBoxFFun(t, ff)) return makeBlockSchema(t, ffarity(ff), 1, ffname(ff), NormalColor);

    Tree label, chan, type, name, file;
    if (isBoxFConst(t, type, name, file) || isBoxFVar(t, type, name, file)) return makeBlockSchema(t, 0, 1, tree2str(name), NormalColor);
    if (isBoxButton(t) || isBoxCheckbox(t) || isBoxVSlider(t) || isBoxHSlider(t) || isBoxNumEntry(t)) return makeBlockSchema(t, 0, 1, userInterfaceDescription(t), UiColor);
    if (isBoxVBargraph(t) || isBoxHBargraph(t)) return makeBlockSchema(t, 1, 1, userInterfaceDescription(t), UiColor);
    if (isBoxSoundfile(t, label, chan)) return makeBlockSchema(t, 2, 2 + tree2int(chan), userInterfaceDescription(t), UiColor);

    Tree a, b;
    if (isBoxMetadata(t, a, b)) return createSchema(a);

    const bool isVGroup = isBoxVGroup(t, label, a);
    const bool isHGroup = isBoxHGroup(t, label, a);
    const bool isTGroup = isBoxTGroup(t, label, a);
    if (isVGroup || isHGroup || isTGroup) {
        const string groupId = isVGroup ? "v" : isHGroup ? "h" : "t";
        return new DecorateSchema(a, createSchema(a), groupId + "group(" + extractName(label) + ")");
    }
    if (isBoxSeq(t, a, b)) return makeSequentialSchema(t, createSchema(a), createSchema(b));
    if (isBoxPar(t, a, b)) return makeParallelSchema(t, createSchema(a), createSchema(b));
    if (isBoxSplit(t, a, b)) return new SplitSchema(t, makeEnlargedSchema(createSchema(a), dWire), makeEnlargedSchema(createSchema(b), dWire));
    if (isBoxMerge(t, a, b)) return new MergeSchema(t, makeEnlargedSchema(createSchema(a), dWire), makeEnlargedSchema(createSchema(b), dWire));
    if (isBoxRec(t, a, b)) {
        // The smaller component is enlarged to the width of the other.
        auto *s1 = createSchema(a);
        auto *s2 = createSchema(b);
        auto *s1e = makeEnlargedSchema(s1, s2->width);
        auto *s2e = makeEnlargedSchema(s2, s1->width);
        const float w = s1e->width + 2 * dWire * float(max(s2e->inputs, s2e->outputs));
        return new RecursiveSchema(t, s1e, s2e, w);
    }
    if (isBoxSlot(t, &i)) return makeBlockSchema(t, 0, 1, getTreeName(t), SlotColor);

    if (isBoxSymbolic(t, a, b)) {
        auto *abstractionSchema = generateAbstractionSchema(generateInputSlotSchema(a), b);

        Tree id;
        if (getDefNameProperty(t, id)) return abstractionSchema;
        return new DecorateSchema(t, abstractionSchema, "Abstraction");
    }
    if (isBoxEnvironment(t)) return makeBlockSchema(t, 0, 0, "environment{...}", NormalColor);

    Tree c;
    if (isBoxRoute(t, a, b, c)) {
        int ins, outs;
        vector<int> route;
        if (isBoxInt(a, &ins) && isBoxInt(b, &outs) && isIntTree(c, route)) {
            // Build n x m cable routing
            const float minimal = 3 * dWire;
            const float h = 2 * dVert + max(minimal, max(float(ins), float(outs)) * dWire);
            const float w = 2 * dHorz + max(minimal, h * 0.75f);
            return new RouteSchema(t, ins, outs, w, h, route);
        }

        throw std::runtime_error((stringstream("Invalid route expression : ") << boxpp(t)).str());
    }

    throw std::runtime_error((stringstream("ERROR in generateInsideSchema, box expression not recognized: ") << boxpp(t)).str());
}

// Returns `true` if the tree is only made of cut, wires and slots.
static bool isPureRouting(Tree t) {
    if (dc->isTreePureRouting.contains(t)) return dc->isTreePureRouting[t];

    Tree x, y;
    if (isBoxCut(t) || isBoxWire(t) || isInverter(t) || isBoxSlot(t) || (isBoxBinary(t, x, y) && isPureRouting(x) && isPureRouting(y))) {
        dc->isTreePureRouting.emplace(t, true);
        return true;
    }

    dc->isTreePureRouting.emplace(t, false);
    return false;
}

static bool GlobalNoBlock = false;

static Schema *createSchema(Tree t, bool no_block) {
    if (const char *name = getTreeName(t)) {
        Tree parentTree = dc->treeFocusHierarchy.empty() ? nullptr : dc->treeFocusHierarchy.top();
        dc->treeFocusHierarchy.push(t);
        auto *schema = new DecorateSchema{t, generateInsideSchema(t), name, parentTree};
        dc->treeFocusHierarchy.pop();
        if (!GlobalNoBlock && !no_block && schema->topLevel) {
            int ins, outs;
            getBoxType(t, &ins, &outs);
            return makeBlockSchema(t, ins, outs, name, LinkColor, schema);
        }
        if (!isPureRouting(t)) return schema; // Draw a line around the object with its name.
    }

    return generateInsideSchema(t); // normal case
}

void drawBox(Box box) {
    fs::remove_all(faustDiagramsPath);
    fs::create_directory(faustDiagramsPath);
    dc = std::make_unique<DrawContext>();

    auto *schema = createSchema(box, true);
    schema->place();
    schema->draw(SVGDeviceType);
}

void Audio::Faust::Diagram::draw() const {
    if (!c.faust_box) return;

    dc = std::make_unique<DrawContext>();
    // todo only create/place new schema when `c.faust_box` changes
    GlobalNoBlock = true;
    auto *schema = createSchema(c.faust_box);
    GlobalNoBlock = false;
    schema->place();
    ImGui::BeginChild("Diagram", {schema->width, schema->height}, false, ImGuiWindowFlags_HorizontalScrollbar);
    schema->draw(ImGuiDeviceType);
    ImGui::EndChild();
}
