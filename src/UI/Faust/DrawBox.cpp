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
#include "signals/prim2.hh"
#include "faust/dsp/libfaust-signal.h"

#include "../../Helper/assert.h"

using namespace fmt;

static const fs::path FaustDiagramsPath = "FaustDiagrams"; // todo app property

// todo style props
static const int FoldComplexity = 2; // Number of boxes within a `Schema` before folding
static const bool IsSvgScaled = false; // Draw scaled SVG files
static const float BinarySchemaHorizontalGapRatio = 4;
static const bool SequentialConnectionZigzag = true; // false allows for diagonal lines instead of zigzags instead of zigzags
static const bool DrawRouteFrame = false;
static const float TopSchemaMargin = 10;
static const float DecorateSchemaMargin = 10;
static const float DecorateSchemaLabelOffset = 5;
static const float WireGap = 8;
static const float LetterWidth = 4.3; //  todo derive using ImGui for ImGui rendering (but keep for SVG rendering)
static const float XGap = 4;
static const float YGap = 4;
static const float InverterRadius = 1.5;

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
    SVGDevice(string file_name, float w, float h) : file_name(std::move(file_name)) {
        static const float scale = 0.5;
        stream << format(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {} {}")", w, h);
        stream << (IsSvgScaled ? R"( width="100%" height="100%">)" : format(R"( width="{}mm" height="{}mm">)", w * scale, h * scale));
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
        const auto top_left = ImVec2{x, y};
        const auto top_right = top_left + ImVec2{w, 0};
        const auto bottom_left = top_left + ImVec2{0, h};
        const auto bottom_right = bottom_left + ImVec2{w, 0};
        const float text_left = x + DecorateSchemaLabelOffset;

        stream << dash_line(top_left, bottom_left); // left line
        stream << dash_line(bottom_left, bottom_right); // bottom line
        stream << dash_line(bottom_right, top_right); // right line
        stream << dash_line(top_left, {text_left, top_left.y}); // top segment before text
        stream << dash_line({min(text_left + float(1 + text.size()) * LetterWidth * 0.75f, bottom_right.x), top_left.y}, {bottom_right.x, top_left.y}); // top segment after text

        stream << label({text_left, top_left.y}, text);
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

static ImU32 convert_color(const string &color) {
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
//        drawList->AddRectFilled({x, y}, {x + w, y + h}, convert_color(color));
        draw_list->AddRectFilled(pos + ImVec2{x, y}, pos + ImVec2{x + w, y + h}, ImGui::GetColorU32(ImGuiCol_Button));
    }

    void grouprect(const ImVec4 &rect, const string &text) override {
        const auto [x, y, w, h] = rect;
        const ImVec2 text_pos = {x + DecorateSchemaLabelOffset, y};
        draw_list->AddRect(pos + ImVec2{x, y}, pos + ImVec2{x + w, y + h}, ImGui::GetColorU32(ImGuiCol_Border));
        draw_list->AddText(pos + text_pos, ImGui::GetColorU32(ImGuiCol_Text), text.c_str());
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
    const string &tree_name = getTreeName(t);
    if (tree_name == "process") return tree_name + ".svg";
    return (views::take_while(tree_name, [](char c) { return std::isalnum(c); }) | views::take(16) | to<string>)
        + format("-{:x}", reinterpret_cast<std::uintptr_t>(t)) + ".svg";
}

// An abstract block diagram schema
struct Schema {
    Tree tree;
    const Count in_count, out_count;
    const float w, h;
    const std::vector<Schema *> children{};
    const Count descendents = 0; // The number of boxes within this schema (recursively).
    const bool is_top_level;
    Tree parent;

    // Fields populated in `place()`:
    float x = 0, y = 0;
    Orientation orientation = LeftRight;

    Schema(Tree t, Count in_count, Count out_count, float w, float h, std::vector<Schema *> children = {}, Count directDescendents = 0, Tree parent = nullptr)
        : tree(t), in_count(in_count), out_count(out_count), w(w), h(h), children(std::move(children)),
          descendents(directDescendents + ::ranges::accumulate(this->children | views::transform([](Schema *child) { return child->descendents; }), 0)),
          is_top_level(descendents >= FoldComplexity), parent(parent) {}
    virtual ~Schema() = default;

    void place(float new_x, float new_y, Orientation new_orientation) {
        x = new_x;
        y = new_y;
        orientation = new_orientation;
        _place();
    }
    void place() { _place(); }
    void draw(Device &device) const {
        for (const auto *child: children) child->draw(device);
        _draw(device);
    };
    virtual ImVec2 input_point(Count i) const = 0;
    virtual ImVec2 output_point(Count i) const = 0;
    inline bool is_lr() const { return orientation == LeftRight; }

    void draw(DeviceType type) const {
        if (type == SVGDeviceType) {
            SVGDevice device(FaustDiagramsPath / svgFileName(tree), w, h);
            device.rect({x, y, w - 1, h - 1}, "#ffffff", svgFileName(parent));
            draw(device);
        } else {
            ImGuiDevice device;
            draw(device);
        }
    }

    inline float right() const { return x + w; }
    inline float bottom() const { return y + h; }
    inline ImVec2 position() const { return {x, y}; }
    inline ImVec2 size() const { return {w, h}; }
    inline ImVec2 mid() const { return {x + w / 2, y + h / 2}; }
    inline ImRect rect() const { return {{x, y}, {x + w, y + h}}; }
    inline ImVec4 xywh() const { return {x, y, w, h}; }

protected:
    virtual void _place() = 0;
    virtual void _draw(Device &) const {};
};

struct IOSchema : Schema {
    IOSchema(Tree t, Count in_count, Count out_count, float w, float h, std::vector<Schema *> children = {}, Count directDescendents = 0, Tree parent = nullptr)
        : Schema(t, in_count, out_count, w, h, std::move(children), directDescendents, parent) {}

    void _place() override {
        const float dy = is_lr() ? WireGap : -WireGap;
        for (Count i = 0; i < in_count; i++) {
            const float in_y = mid().y - WireGap * float(in_count - 1) / 2;
            input_points[i] = {x + (is_lr() ? 0 : w), in_y + float(i) * dy};
        }
        for (Count i = 0; i < out_count; i++) {
            const float out_y = mid().y - WireGap * float(out_count - 1) / 2;
            output_points[i] = {x + (is_lr() ? w : 0), out_y + float(i) * dy};
        }
    }

    ImVec2 input_point(Count i) const override { return input_points[i]; }
    ImVec2 output_point(Count i) const override { return output_points[i]; }

    std::vector<ImVec2> input_points{in_count};
    std::vector<ImVec2> output_points{out_count};
};

static inline float quantize(int n) {
    static const int q = 3;
    return float(q * ((n + q - 1) / q)); // NOLINT(bugprone-integer-division)
}

// A simple rectangular box with text and inputs/outputs.
struct BlockSchema : IOSchema {
    BlockSchema(Tree t, Count in_count, Count out_count, float w, float h, string text, string color, Schema *inner = nullptr)
        : IOSchema(t, in_count, out_count, w, h, {}, 1),
          text(std::move(text)), color(std::move(color)), inner(inner) {}
    BlockSchema(Tree t, Count in_count, Count out_count, string text, string color, Schema *inner = nullptr)
        : BlockSchema(t, in_count, out_count,
        2 * XGap + max(3 * WireGap, LetterWidth * quantize(int(text.size()))),
        2 * YGap + max(3 * WireGap, float(max(in_count, out_count)) * WireGap),
        std::move(text), std::move(color), inner) {}

    void _place() override {
        IOSchema::_place();
        if (inner) inner->place();
    }

    void _draw(Device &device) const override {
        if (inner) inner->draw(device.type());
        const string &link = inner ? svgFileName(tree) : "";
        device.rect(xywh() + ImVec4{XGap, YGap, -2 * XGap, -2 * YGap}, color, link);
        device.text(position() + size() / 2, text, link);

        // Draw a small point that indicates the first input (like an integrated circuits).
        device.dot(position() + (is_lr() ? ImVec2{XGap, YGap} : ImVec2{w - XGap, h - YGap}), orientation);
        draw_connections(device);
    }

    void draw_connections(Device &device) const {
        const ImVec2 d = {is_lr() ? XGap : -XGap, 0};
        for (const auto &p: input_points) device.line(p, p + d); // Input lines
        for (const auto &p: output_points) device.line(p - d, p); // Output lines
        for (const auto &p: input_points) device.arrow(p + d, 0, orientation); // Input arrows
    }

    const string text, color;
    Schema *inner;
};

// Simple cables (identity box) in parallel.
// The width of a cable is null.
// Therefor, input and output connection points are the same.
struct CableSchema : Schema {
    CableSchema(Tree t, Count n = 1) : Schema(t, n, n, 0, float(n) * WireGap) {}

    // Place the communication points vertically spaced by `WireGap`.
    void _place() override {
        for (Count i = 0; i < in_count; i++) {
            const float dx = WireGap * (float(i) + 0.5f);
            points[i] = {x, y + (is_lr() ? dx : h - dx)};
        }
    }

    ImVec2 input_point(Count i) const override { return points[i]; }
    ImVec2 output_point(Count i) const override { return points[i]; }

private:
    std::vector<ImVec2> points{in_count};
};

// An inverter is a circle followed by a triangle.
// It corresponds to '*(-1)', and it's used to create more compact diagrams.
struct InverterSchema : BlockSchema {
    InverterSchema(Tree t) : BlockSchema(t, 1, 1, 2.5f * WireGap, WireGap, "-1", InverterColor) {}

    void _draw(Device &device) const override {
        const float x1 = w - 2 * XGap;
        const float y1 = 0.5f + (h - 1) / 2;
        const auto tri_a = position() + ImVec2{XGap + (is_lr() ? 0 : x1), 0};
        const auto tri_b = tri_a + ImVec2{(is_lr() ? x1 - 2 * InverterRadius : 2 * InverterRadius - x1 - x), y1};
        const auto tri_c = tri_a + ImVec2{0, h - 1};
        device.circle(tri_b + ImVec2{is_lr() ? InverterRadius : -InverterRadius, 0}, InverterRadius, color);
        device.triangle(tri_a, tri_b, tri_c, color);
        draw_connections(device);
    }
};

// Cable termination
struct CutSchema : Schema {
    // A Cut is represented by a small black dot.
    // It has 1 input and no out_count.
    // It has 0 width and 1 height, for the wire.
    CutSchema(Tree t) : Schema(t, 1, 0, 0, WireGap / 100.0f) {}

    void _place() override {}

    // A cut is represented by a small black dot.
    void _draw(Device &) const override {
        // device.circle(point, WireGap / 8);
    }

    // A Cut has only one input point
    ImVec2 input_point(Count) const override { return {x, mid().y}; }

    // A Cut has no output point.
    ImVec2 output_point(Count) const override {
        fgassert(false);
        return {-1, -1};
    }
};

// todo can this just be a method returning a `Rect` on `Schema`?
struct EnlargedSchema : IOSchema {
    EnlargedSchema(Schema *s, float w) : IOSchema(s->tree, s->in_count, s->out_count, w, s->h, {s}) {}

    void _place() override {
        auto *schema = children[0];
        const float dx = (w - schema->w) / 2;
        schema->place(x + dx, y, orientation);

        const ImVec2 d = {is_lr() ? dx : -dx, 0};
        for (Count i = 0; i < in_count; i++) input_points[i] = schema->input_point(i) - d;
        for (Count i = 0; i < out_count; i++) output_points[i] = schema->output_point(i) + d;
    }

    void _draw(Device &device) const override {
        const auto &schema = children[0];
        for (Count i = 0; i < in_count; i++) device.line(input_point(i), schema->input_point(i));
        for (Count i = 0; i < out_count; i++) device.line(schema->output_point(i), output_point(i));
    }
};

struct BinarySchema : Schema {
    BinarySchema(Tree t, Schema *s1, Schema *s2, Count in_count, Count out_count, float horzGap, float w, float h)
        : Schema(t, in_count, out_count, w, h, {s1, s2}), horzGap(horzGap) {}
    BinarySchema(Tree t, Schema *s1, Schema *s2, Count in_count, Count out_count, float horzGap)
        : BinarySchema(t, s1, s2, in_count, out_count, horzGap, s1->w + s2->w + horzGap, max(s1->h, s2->h)) {}
    BinarySchema(Tree t, Schema *s1, Schema *s2, float horzGap) : BinarySchema(t, s1, s2, s1->in_count, s2->out_count, horzGap) {}
    BinarySchema(Tree t, Schema *s1, Schema *s2) : BinarySchema(t, s1, s2, s1->in_count, s2->out_count, (s1->h + s2->h) / BinarySchemaHorizontalGapRatio) {}

    ImVec2 input_point(Count i) const override { return children[0]->input_point(i); }
    ImVec2 output_point(Count i) const override { return children[1]->output_point(i); }

    // Place the two components horizontally, centered, with enough space for the connections.
    void _place() override {
        auto *leftSchema = children[is_lr() ? 0 : 1];
        auto *rightSchema = children[is_lr() ? 1 : 0];
        const float dy1 = max(0.0f, rightSchema->h - leftSchema->h) / 2;
        const float dy2 = max(0.0f, leftSchema->h - rightSchema->h) / 2;
        leftSchema->place(x, y + dy1, orientation);
        rightSchema->place(x + leftSchema->w + horzGap, y + dy2, orientation);
    }

    float horzGap;
};

struct ParallelSchema : BinarySchema {
    ParallelSchema(Tree t, Schema *s1, Schema *s2)
        : BinarySchema(t, s1, s2, s1->in_count + s2->in_count, s1->out_count + s2->out_count, 0, s1->w, s1->h + s2->h),
          in_frontier(s1->in_count), out_frontier(s1->out_count) {
        fgassert(s1->w == s2->w);
    }

    void _place() override {
        auto *top_schema = children[is_lr() ? 0 : 1];
        auto *bottom_schema = children[is_lr() ? 1 : 0];
        top_schema->place(x, y, orientation);
        bottom_schema->place(x, y + top_schema->h, orientation);
    }

    ImVec2 input_point(Count i) const override { return i < in_frontier ? children[0]->input_point(i) : children[1]->input_point(i - in_frontier); }
    ImVec2 output_point(Count i) const override { return i < out_frontier ? children[0]->output_point(i) : children[1]->output_point(i - out_frontier); }

private:
    Count in_frontier, out_frontier;
};

struct SequentialSchema : BinarySchema {
    // The components s1 and s2 must be "compatible" (s1: n->m and s2: m->q).
    SequentialSchema(Tree t, Schema *s1, Schema *s2) : BinarySchema(t, s1, s2, horizontal_gap(s1, s2)) {
        fgassert(s1->out_count == s2->in_count);
    }

    static ImGuiDir connection_direction(const ImVec2 &a, const ImVec2 &b) {
        // Remember that the y-axis goes from top to bottom.
        return a.y > b.y ? ImGuiDir_Up : (a.y < b.y ? ImGuiDir_Down : ImGuiDir_Right);
    }

    void _draw(Device &device) const override {
        BinarySchema::_draw(device);

        // Draw the internal wires aligning the vertical segments in a symmetric way when possible.
        float dx = 0, mx = 0;
        ImGuiDir direction = ImGuiDir_None;
        for (Count i = 0; i < children[0]->out_count; i++) {
            const auto from = children[0]->output_point(i);
            const auto to = children[1]->input_point(i);
            const ImGuiDir d = connection_direction(from, to);
            if (d == direction) {
                mx += dx; // Move in same direction.
            } else {
                mx = is_lr() ? (d == ImGuiDir_Down ? horzGap : 0) : (d == ImGuiDir_Up ? -horzGap : 0);
                dx = d == ImGuiDir_Up ? WireGap : d == ImGuiDir_Down ? -WireGap : 0;
                direction = d;
            }
            if (!SequentialConnectionZigzag || from.y == to.y) {
                device.line(from, to); // Draw a straight, potentially diagonal cable.
            } else {
                // Draw a zigzag cable by traversing half the distance between, taking a sharp turn, then turning back and finishing.
                device.line(from, {from.x + mx, from.y});
                device.line({from.x + mx, from.y}, {from.x + mx, to.y});
                device.line({from.x + mx, to.y}, to);
            }
        }
    }

    // Compute the horizontal gap needed to draw the internal wires.
    // It depends on the largest group of connections that go in the same direction.
    // **Side effect: May add cables to ensure the internal connections are between the same number of ins/outs.**
    static float horizontal_gap(Schema *a, Schema *b) {
        if (a->out_count == 0) return 0;

        a->place(0, max(0.0f, b->h - a->h) / 2, LeftRight);
        b->place(0, max(0.0f, a->h - b->h) / 2, LeftRight);

        ImGuiDir direction = ImGuiDir_None;
        Count size = 0;
        std::map<ImGuiDir, Count> MaxGroupSize; // store the size of the largest group for each direction
        for (Count i = 0; i < a->out_count; i++) {
            const auto d = connection_direction(a->output_point(i), b->input_point(i));
            size = (d == direction ? size + 1 : 1);
            direction = d;
            MaxGroupSize[direction] = max(MaxGroupSize[direction], size);
        }

        return WireGap * float(max(MaxGroupSize[ImGuiDir_Up], MaxGroupSize[ImGuiDir_Down]));
    }
};

// Place and connect two diagrams in merge composition.
// The outputs of the first schema are merged to the inputs of the second.
struct MergeSchema : BinarySchema {
    MergeSchema(Tree t, Schema *s1, Schema *s2) : BinarySchema(t, s1, s2) {}

    void _draw(Device &device) const override {
        BinarySchema::_draw(device);
        for (Count i = 0; i < children[0]->out_count; i++) device.line(children[0]->output_point(i), children[1]->input_point(i % children[1]->in_count));
    }
};

// Place and connect two diagrams in split composition.
// The outputs the first schema are distributed to the inputs of the second.
struct SplitSchema : BinarySchema {
    SplitSchema(Tree t, Schema *s1, Schema *s2) : BinarySchema(t, s1, s2) {}

    void _draw(Device &device) const override {
        BinarySchema::_draw(device);
        for (Count i = 0; i < children[1]->in_count; i++) device.line(children[0]->output_point(i % children[0]->out_count), children[1]->input_point(i));
    }
};

// Place and connect two diagrams in recursive composition
// The two components must have the same width.
struct RecursiveSchema : IOSchema {
    RecursiveSchema(Tree t, Schema *s1, Schema *s2, float w)
        : IOSchema(t, s1->in_count - s2->out_count, s1->out_count, w, s1->h + s2->h, {s1, s2}) {
        fgassert(s1->in_count >= s2->out_count);
        fgassert(s1->out_count >= s2->in_count);
        fgassert(s1->w >= s2->w);
    }

    // The two schemas are centered vertically, stacked on top of each other, with stacking order dependent on orientation.
    void _place() override {
        auto *top_schema = children[is_lr() ? 1 : 0];
        auto *bottom_schema = children[is_lr() ? 0 : 1];
        top_schema->place(x + (w - top_schema->w) / 2, y, RightLeft);
        bottom_schema->place(x + (w - bottom_schema->w) / 2, y + top_schema->h, LeftRight);

        const ImVec2 d1 = {(w - children[0]->w * (is_lr() ? 1.0f : -1.0f)) / 2, 0};
        for (Count i = 0; i < in_count; i++) input_points[i] = children[0]->input_point(i + children[1]->out_count) - d1;
        for (Count i = 0; i < out_count; i++) output_points[i] = children[0]->output_point(i) + d1;
    }

    void _draw(Device &device) const override {
        const auto *s1 = children[0];
        const auto *s2 = children[1];
        // Implicit feedback delay to each `s2` input
        const float dw = is_lr() ? WireGap : -WireGap;
        for (Count i = 0; i < s2->in_count; i++) draw_delay_sign(device, s1->output_point(i) + ImVec2{float(i) * dw, 0}, dw / 2);
        // Feedback connections to each `s2` input
        for (Count i = 0; i < s2->in_count; i++) draw_feedback(device, s1->output_point(i), s2->input_point(i), float(i) * WireGap, output_point(i));
        // Non-recursive output lines
        for (Count i = s2->in_count; i < out_count; i++) device.line(s1->output_point(i), output_point(i));
        // Input lines
        for (Count i = 0; i < in_count; i++) device.line(input_point(i), s1->input_point(i + s2->out_count));
        // Feedfront connections from each `s2` output
        for (Count i = 0; i < s2->out_count; i++) draw_feedfront(device, s2->output_point(i), s1->input_point(i), float(i) * WireGap);
    }

private:
    // Draw a feedback connection between two points with a horizontal displacement `dx`.
    void draw_feedback(Device &device, const ImVec2 &from, const ImVec2 &to, float dx, const ImVec2 &out) const {
        const float ox = from.x + (is_lr() ? dx : -dx);
        const float ct = (is_lr() ? WireGap : -WireGap) / 2;
        const ImVec2 up(ox, from.y - ct);
        const ImVec2 br(ox + ct / 2, from.y);

        device.line(up, {ox, to.y});
        device.line({ox, to.y}, to);
        device.line(from, br);
        device.line(br, out);
    }

    // Draw a feedfrom connection between two points with a horizontal displacement `dx`.
    void draw_feedfront(Device &device, const ImVec2 &from, const ImVec2 &to, float dx) const {
        const float dfx = from.x + (is_lr() ? -dx : dx);
        device.line({from.x, from.y}, {dfx, from.y});
        device.line({dfx, from.y}, {dfx, to.y});
        device.line({dfx, to.y}, {to.x, to.y});
    }

    // Draw the delay sign of a feedback connection (three sides of a square)
    static void draw_delay_sign(Device &device, const ImVec2 &pos, float size) {
        const float hs = size / 2;
        device.line(pos - ImVec2{hs, 0}, pos - ImVec2{hs, size});
        device.line(pos - ImVec2{hs, size}, pos + ImVec2{hs, -size});
        device.line(pos + ImVec2{hs, -size}, pos + ImVec2{hs, 0});
    }
};

Schema *enlarge(Schema *schema, float w) { return w > schema->w ? new EnlargedSchema(schema, w) : schema; }
Schema *make_parallel(Tree t, Schema *s1, Schema *s2) { return new ParallelSchema(t, enlarge(s1, s2->w), enlarge(s2, s1->w)); }
Schema *make_sequential(Tree t, Schema *s1, Schema *s2) {
    const auto o = s1->out_count;
    const auto i = s2->in_count;
    return new SequentialSchema(t,
        o < i ? make_parallel(t, s1, new CableSchema(t, i - o)) : s1,
        o > i ? make_parallel(t, s2, new CableSchema(t, o - i)) : s2
    );
}

// A `DecorateSchema` is a schema surrounded by a dashed rectangle with a label on the top left, and arrows added to the outputs.
// If the number of boxes inside is over the `box_complexity` threshold, add additional padding and draw output arrows.
struct DecorateSchema : IOSchema {
    DecorateSchema(Tree t, Schema *inner, string text, Tree parent = nullptr)
        : IOSchema(t, inner->in_count, inner->out_count,
        inner->w + 2 * (DecorateSchemaMargin + (inner->is_top_level ? TopSchemaMargin : 0)),
        inner->h + 2 * (DecorateSchemaMargin + (inner->is_top_level ? TopSchemaMargin : 0)),
        {inner}, 0, parent), text(std::move(text)) {}

    void _place() override {
        const float margin = DecorateSchemaMargin + (is_top_level ? TopSchemaMargin : 0);
        const auto &schema = children[0];
        schema->place(x + margin, y + margin, orientation);

        const ImVec2 m = {is_lr() ? TopSchemaMargin : -TopSchemaMargin, 0};
        for (Count i = 0; i < in_count; i++) input_points[i] = schema->input_point(i) - m;
        for (Count i = 0; i < out_count; i++) output_points[i] = schema->output_point(i) + m;
    }

    void _draw(Device &device) const override {
        const auto &schema = children[0];
        const float top_level_margin = is_top_level ? TopSchemaMargin : 0;
        const float margin = 2 * top_level_margin + DecorateSchemaMargin;
        const auto rect_pos = position() + ImVec2{margin, margin} / 2;
        const auto rect_size = size() - ImVec2{margin, margin};
        device.grouprect({rect_pos.x, rect_pos.y, rect_size.x, rect_size.y}, text);
        for (Count i = 0; i < in_count; i++) device.line(input_point(i), schema->input_point(i));
        for (Count i = 0; i < out_count; i++) device.line(schema->output_point(i), output_point(i));

        if (is_top_level) for (Count i = 0; i < out_count; i++) device.arrow(output_point(i), 0, orientation);
    }

private:
    string text;
};

struct RouteSchema : IOSchema {
    RouteSchema(Tree t, Count in_count, Count out_count, float w, float h, std::vector<int> routes)
        : IOSchema(t, in_count, out_count, w, h), color("#EEEEAA"), routes(std::move(routes)) {}

    void _draw(Device &device) const override {
        if (DrawRouteFrame) {
            device.rect(xywh() + ImVec4{XGap, YGap, -2 * XGap, -2 * YGap}, color, "");
            // Draw the orientation mark, a small point that indicates the first input (like integrated circuits).
            device.dot(position() + (is_lr() ? ImVec2{XGap, YGap} : ImVec2{w - XGap, h - YGap}), orientation);
            // Input arrows
            for (const auto &p: input_points) device.arrow(p + ImVec2{is_lr() ? XGap : -XGap, 0}, 0, orientation);
        }

        // Input/output & route wires
        const auto d = ImVec2{is_lr() ? XGap : -XGap, 0};
        for (const auto &p: input_points) device.line(p, p + d);
        for (const auto &p: output_points) device.line(p - d, p);
        for (Count i = 0; i < routes.size() - 1; i += 2) device.line(input_points[routes[i] - 1] + d, output_points[routes[i + 1] - 1] - d);
    }

protected:
    const string color;
    const std::vector<int> routes;  // Route description: s1,d2,s2,d2,...
};

static bool isBoxBinary(Tree t, Tree &x, Tree &y) {
    return isBoxPar(t, x, y) || isBoxSeq(t, x, y) || isBoxSplit(t, x, y) || isBoxMerge(t, x, y) || isBoxRec(t, x, y);
}

static Schema *Tree2Schema(Tree t, bool allow_links = true);

// Generate a 1->0 block schema for an input slot.
static Schema *make_input_slot(Tree t) { return new BlockSchema(t, 1, 0, getTreeName(t), SlotColor); }

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

// Generate an abstraction schema by placing in sequence the input slots and the body.
static Schema *make_abstraction(Schema *x, Tree t) {
    Tree a, b;
    while (isBoxSymbolic(t, a, b)) {
        x = make_parallel(t, x, make_input_slot(a));
        t = b;
    }
    return make_sequential(t, x, Tree2Schema(t));
}

// Generate the inside schema of a block diagram according to its type.
static Schema *Tree2SchemaNode(Tree t) {
    if (getUserData(t) != nullptr) return new BlockSchema(t, xtendedArity(t), 1, xtendedName(t), NormalColor);
    if (isInverter(t)) return new InverterSchema(t);

    int i;
    double r;
    if (isBoxInt(t, &i) || isBoxReal(t, &r)) return new BlockSchema(t, 0, 1, isBoxInt(t) ? std::to_string(i) : std::to_string(r), NumberColor);
    if (isBoxWaveform(t)) return new BlockSchema(t, 0, 2, "waveform{...}", NormalColor);
    if (isBoxWire(t)) return new CableSchema(t);
    if (isBoxCut(t)) return new CutSchema(t);

    prim0 p0;
    prim1 p1;
    prim2 p2;
    prim3 p3;
    prim4 p4;
    prim5 p5;
    if (isBoxPrim0(t, &p0)) return new BlockSchema(t, 0, 1, prim0name(p0), NormalColor);
    if (isBoxPrim1(t, &p1)) return new BlockSchema(t, 1, 1, prim1name(p1), NormalColor);
    if (isBoxPrim2(t, &p2)) return new BlockSchema(t, 2, 1, prim2name(p2), NormalColor);
    if (isBoxPrim3(t, &p3)) return new BlockSchema(t, 3, 1, prim3name(p3), NormalColor);
    if (isBoxPrim4(t, &p4)) return new BlockSchema(t, 4, 1, prim4name(p4), NormalColor);
    if (isBoxPrim5(t, &p5)) return new BlockSchema(t, 5, 1, prim5name(p5), NormalColor);

    Tree ff;
    if (isBoxFFun(t, ff)) return new BlockSchema(t, ffarity(ff), 1, ffname(ff), NormalColor);

    Tree label, chan, type, name, file;
    if (isBoxFConst(t, type, name, file) || isBoxFVar(t, type, name, file)) return new BlockSchema(t, 0, 1, tree2str(name), NormalColor);
    if (isBoxButton(t) || isBoxCheckbox(t) || isBoxVSlider(t) || isBoxHSlider(t) || isBoxNumEntry(t)) return new BlockSchema(t, 0, 1, userInterfaceDescription(t), UiColor);
    if (isBoxVBargraph(t) || isBoxHBargraph(t)) return new BlockSchema(t, 1, 1, userInterfaceDescription(t), UiColor);
    if (isBoxSoundfile(t, label, chan)) return new BlockSchema(t, 2, 2 + tree2int(chan), userInterfaceDescription(t), UiColor);

    Tree a, b;
    if (isBoxMetadata(t, a, b)) return Tree2Schema(a);

    const bool isVGroup = isBoxVGroup(t, label, a);
    const bool isHGroup = isBoxHGroup(t, label, a);
    const bool isTGroup = isBoxTGroup(t, label, a);
    if (isVGroup || isHGroup || isTGroup) {
        const string groupId = isVGroup ? "v" : isHGroup ? "h" : "t";
        return new DecorateSchema(a, Tree2Schema(a), groupId + "group(" + extractName(label) + ")");
    }
    if (isBoxSeq(t, a, b)) return make_sequential(t, Tree2Schema(a), Tree2Schema(b));
    if (isBoxPar(t, a, b)) return make_parallel(t, Tree2Schema(a), Tree2Schema(b));
    if (isBoxSplit(t, a, b)) return new SplitSchema(t, enlarge(Tree2Schema(a), WireGap), enlarge(Tree2Schema(b), WireGap));
    if (isBoxMerge(t, a, b)) return new MergeSchema(t, enlarge(Tree2Schema(a), WireGap), enlarge(Tree2Schema(b), WireGap));
    if (isBoxRec(t, a, b)) {
        // The smaller component is enlarged to the width of the other.
        auto *s1 = Tree2Schema(a);
        auto *s2 = Tree2Schema(b);
        auto *s1e = enlarge(s1, s2->w);
        auto *s2e = enlarge(s2, s1->w);
        const float w = s1e->w + 2 * WireGap * float(max(s2e->in_count, s2e->out_count));
        return new RecursiveSchema(t, s1e, s2e, w);
    }
    if (isBoxSlot(t, &i)) return new BlockSchema(t, 0, 1, getTreeName(t), SlotColor);

    if (isBoxSymbolic(t, a, b)) {
        auto *abstractionSchema = make_abstraction(make_input_slot(a), b);

        Tree id;
        if (getDefNameProperty(t, id)) return abstractionSchema;
        return new DecorateSchema(t, abstractionSchema, "Abstraction");
    }
    if (isBoxEnvironment(t)) return new BlockSchema(t, 0, 0, "environment{...}", NormalColor);

    Tree c;
    if (isBoxRoute(t, a, b, c)) {
        int ins, outs;
        vector<int> route;
        if (isBoxInt(a, &ins) && isBoxInt(b, &outs) && isIntTree(c, route)) {
            // Build n x m cable routing
            const float minimal = 3 * WireGap;
            const float h = 2 * YGap + max(minimal, max(float(ins), float(outs)) * WireGap);
            const float w = 2 * XGap + max(minimal, h * 0.75f);
            return new RouteSchema(t, ins, outs, w, h, route);
        }

        throw std::runtime_error((stringstream("Invalid route expression : ") << boxpp(t)).str());
    }

    throw std::runtime_error((stringstream("ERROR in Tree2SchemaNode, box expression not recognized: ") << boxpp(t)).str());
}

static bool AllowSchemaLinks = false; // Set to `false` to draw all schemas inline in one big diagram. Set to `true` to split into files (for SVG rendering).
static std::map<Tree, bool> IsTreePureRouting{}; // Avoid recomputing pure-routing property. Needs to be reset whenever box changes!

// Returns `true` if the tree is only made of cut, wires and slots.
static bool isPureRouting(Tree t) {
    if (IsTreePureRouting.contains(t)) return IsTreePureRouting[t];

    Tree x, y;
    if (isBoxCut(t) || isBoxWire(t) || isInverter(t) || isBoxSlot(t) || (isBoxBinary(t, x, y) && isPureRouting(x) && isPureRouting(y))) {
        IsTreePureRouting.emplace(t, true);
        return true;
    }

    IsTreePureRouting.emplace(t, false);
    return false;
}

// This method is called recursively.
static Schema *Tree2Schema(Tree t, bool allow_links) {
    static std::stack<Tree> treeFocusHierarchy; // As we descend into the tree, keep track of ancestors for backlinks.
    if (const char *name = getTreeName(t)) {
        Tree parent = treeFocusHierarchy.empty() ? nullptr : treeFocusHierarchy.top();
        treeFocusHierarchy.push(t);
        auto *schema = new DecorateSchema{t, Tree2SchemaNode(t), name, parent};
        treeFocusHierarchy.pop();
        if (AllowSchemaLinks && allow_links && schema->is_top_level) {
            int ins, outs;
            getBoxType(t, &ins, &outs);
            return new BlockSchema(t, ins, outs, name, LinkColor, schema);
        }
        if (!isPureRouting(t)) return schema; // Draw a line around the object with its name.
    }

    return Tree2SchemaNode(t); // normal case
}

Schema *active_schema; // This diagram is drawn every frame if present.

void on_box_change(Box box) {
    IsTreePureRouting = {};
    if (box) {
        // Render SVG diagram(s)
        fs::remove_all(FaustDiagramsPath);
        fs::create_directory(FaustDiagramsPath);
        AllowSchemaLinks = true; // split up complex descendent groups into separate files with links
        auto *svg_schema = Tree2Schema(box, false); // ensure top-level is not compressed into a link
        AllowSchemaLinks = false;
        svg_schema->place();
        svg_schema->draw(SVGDeviceType);

        // Render ImGui diagram
        active_schema = Tree2Schema(box);
        active_schema->place();
    } else {
        active_schema = nullptr;
    }
}

void Audio::Faust::Diagram::draw() const {
    if (!active_schema) return;

    ImGui::BeginChild("Faust diagram", {active_schema->w, active_schema->h}, false, ImGuiWindowFlags_HorizontalScrollbar);
    active_schema->draw(ImGuiDeviceType);
    ImGui::EndChild();
}
