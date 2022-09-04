#include "DrawBox.hh"

#include <sstream>
#include <map>
#include <stack>

#include "../../Context.h"

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/take_while.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/map.hpp>

#include "boxes/ppbox.hh"
#include "signals/prim2.hh"
#include "faust/dsp/libfaust-signal.h"

#include "../../Helper/assert.h"

using namespace fmt;

static const fs::path FaustDiagramsPath = "FaustDiagrams"; // todo app property

// todo style props
static const int FoldComplexity = 2; // Number of boxes within a `Schema` before folding
static const bool IsSvgScaled = false; // Draw scaled SVG files
static const float BinarySchemaHorizontalGapRatio = 1.0f / 4;
static const bool SequentialConnectionZigzag = true; // false allows for diagonal lines instead of zigzags instead of zigzags
static const bool DrawRouteFrame = false;
static const float TopSchemaMargin = 10;
static const float DecorateSchemaMargin = 10;
static const float DecorateSchemaLabelOffset = 5;
static const float WireGap = 8;
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

struct TextStyle {
    enum Justify {
        Left,
        Middle,
        Right,
    };
    enum FontStyle {
        Normal,
        Bold,
        Italic,
    };

    const string color{"#ffffff"};
    const Justify justify{Middle};
    const float padding_right{0};
    const float padding_bottom{0};
    const float font_size{7};
    const FontStyle font_style{Normal};
    const bool top{false};
};

struct RectStyle {
    const string fill_color{"#ffffff"};
    const string stroke_color{"none"};
    const float stroke_width{0};
};

class Device {
public:
    virtual ~Device() = default;
    virtual DeviceType type() = 0;
    virtual void rect(const ImVec4 &rect, const string &color, const string &link) = 0;
    virtual void rect(const ImVec4 &rect, const RectStyle &style) = 0;
    virtual void grouprect(const ImVec4 &rect, const string &text) = 0; // A labeled grouping
    virtual void triangle(const ImVec2 &a, const ImVec2 &b, const ImVec2 &c, const string &color) = 0;
    virtual void circle(const ImVec2 &pos, float radius, const string &color) = 0;
    virtual void arrow(const ImVec2 &pos, Orientation orientation) = 0;
    virtual void line(const ImVec2 &start, const ImVec2 &end) = 0;
    virtual void text(const ImVec2 &pos, const string &text, const TextStyle &style = {}, const string &link = "") = 0;
    virtual void dot(const ImVec2 &pos, Orientation orientation) = 0;
};

// todo this is from Faust, used to calculate text width for SVGs. Need to think about why.
static inline float quantize(int n) {
    static const int q = 3;
    return float(q * ((n + q - 1) / q)); // NOLINT(bugprone-integer-division)
}

struct SVGDevice : Device {
    SVGDevice(string file_name, float w, float h) : file_name(std::move(file_name)) {
        static const float scale = 0.5;
        stream << format(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {} {}")", w, h);
        stream << (IsSvgScaled ? R"( width="100%" height="100%">)" : format(R"( width="{}mm" height="{}mm">)", w * scale, h * scale));
    }

    ~SVGDevice() override {
        stream << end_stream.str();
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
        if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link));
        const auto [x, y, w, h] = rect;
        stream << format(R"(<rect x="{}" y="{}" width="{}" height="{}" rx="0" ry="0" style="stroke:none;fill:{};"/>)", x, y, w, h, color);
        if (!link.empty()) stream << "</a>";
    }

    void rect(const ImVec4 &rect, const RectStyle &style) override {
        const auto &[fill_color, stroke_color, stroke_width] = style;
        const auto [x, y, w, h] = rect;
        stream << format(R"(<rect x="{}" y="{}" width="{}" height="{}" rx="0" ry="0" style="stroke:{};stroke-width={};fill:{};"/>)", x, y, w, h, stroke_color, stroke_width, fill_color);
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
        stream << dash_line({min(text_left + text_size(text).x, bottom_right.x), top_left.y}, {bottom_right.x, top_left.y}); // top segment after text

        stream << label({text_left, top_left.y}, text);
    }

    void triangle(const ImVec2 &a, const ImVec2 &b, const ImVec2 &c, const string &color) override {
        stream << format(R"(<polygon fill="{}" stroke="black" stroke-width=".25" points="{},{} {},{} {},{}"/>)", color, a.x, a.y, b.x, b.y, c.x, c.y);
    }

    void circle(const ImVec2 &pos, float radius, const string &color) override {
        const auto [x, y] = pos;
        stream << format(R"(<circle fill="{}" stroke="black" stroke-width=".25" cx="{}" cy="{}" r="{}"/>)", color, x, y, radius);
    }

    void arrow(const ImVec2 &pos, Orientation orientation) override {
        static const float dx = 3, dy = 1;
        const auto [x, y] = pos;
        const auto x1 = orientation == LeftRight ? x - dx : x + dx;
        stream << rotate_line({x1, y - dy}, pos, 0, x, y);
        stream << rotate_line({x1, y + dy}, pos, 0, x, y);
    }

    void line(const ImVec2 &start, const ImVec2 &end) override {
        const string line_cap = start.x == end.x || start.y == end.y ? "butt" : "round";
        stream << format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}"  style="stroke:black; stroke-linecap:{}; stroke-width:0.25;"/>)", start.x, start.y, end.x, end.y, line_cap);
    }

    void text(const ImVec2 &pos, const string &text, const TextStyle &style, const string &link) override {
        const auto &[color, justify, padding_right, padding_bottom, font_size, font_style, top] = style;
        const string anchor = justify == TextStyle::Left ? "start" : justify == TextStyle::Middle ? "middle" : "end";
        const string font_style_formatted = font_style == TextStyle::FontStyle::Italic ? "italic" : "normal";
        const string font_weight = font_style == TextStyle::FontStyle::Bold ? "bold" : "normal";
        auto &text_stream = top ? end_stream : stream;
        if (!link.empty()) text_stream << format(R"(<a href="{}">)", xml_sanitize(link));
        text_stream << format(R"(<text x="{}" y="{}" font-family="Arial" font-style="{}" font-weight="{}" font-size="{}" text-anchor="{}" fill="{}">{}</text>)",
            pos.x - padding_right, pos.y + 2 - padding_bottom, font_style_formatted, font_weight, font_size, anchor, color, xml_sanitize(text));
        if (!link.empty()) text_stream << "</a>";
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

    static ImVec2 text_size(const string &text) {
        static const float LetterWidth = 4.3;
        return {LetterWidth * quantize(int(text.size())), 7};
    }

private:
    string file_name;
    std::stringstream stream;
    std::stringstream end_stream;
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
        draw_list->AddRectFilled(pos + ImVec2{x, y}, pos + ImVec2{x + w, y + h}, ImGui::GetColorU32(ImGuiCol_Button));
    }

    void rect(const ImVec4 &rect, const RectStyle &style) override {
//        const auto &[fill_color, stroke_color, stroke_width] = style;
//        const auto [x, y, w, h] = rect;
//        draw_list->AddRectFilled(pos + ImVec2{x, y}, pos + ImVec2{x + w, y + h}, ImGui::GetColorU32(ImGuiCol_Button));
    }

    void grouprect(const ImVec4 &rect, const string &text) override {
        const auto [x, y, w, h] = rect;
        const ImVec2 text_pos = {x + DecorateSchemaLabelOffset, y - ImGui::GetFontSize() / 2};
        draw_list->AddRect(pos + ImVec2{x, y}, pos + ImVec2{x + w, y + h}, ImGui::GetColorU32(ImGuiCol_Border));
        draw_list->AddText(pos + text_pos, ImGui::GetColorU32(ImGuiCol_Text), text.c_str());
    }

    void triangle(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const string &color) override {
        draw_list->AddTriangle(pos + p1, pos + p2, pos + p3, ImGui::GetColorU32(ImGuiCol_Border));
    }

    void circle(const ImVec2 &p, float radius, const string &color) override {
        draw_list->AddCircle(pos + p, radius, ImGui::GetColorU32(ImGuiCol_Border));
    }

    void arrow(const ImVec2 &p, Orientation orientation) override {
        static const ImVec2 d{6, 2};
        ImGui::RenderArrowPointingAt(draw_list, pos + p, d, orientation == LeftRight ? ImGuiDir_Right : ImGuiDir_Left, ImGui::GetColorU32(ImGuiCol_Border));
    }

    void line(const ImVec2 &start, const ImVec2 &end) override {
        draw_list->AddLine(pos + start, pos + end, ImGui::GetColorU32(ImGuiCol_Border));
    }

    void text(const ImVec2 &p, const string &text, const TextStyle &style, const string &link) override {
        const auto &[color, justify, padding_right, padding_bottom, font_size, font_style, top] = style;
        const auto &text_pos = pos + p - (justify == TextStyle::Left ? ImVec2{} : justify == TextStyle::Middle ? text_size(text) / 2 : text_size(text));
        draw_list->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), text.c_str());
    }

    void dot(const ImVec2 &p, Orientation orientation) override {
        const float offset = orientation == LeftRight ? 2 : -2;
        draw_list->AddCircle(pos + p + ImVec2{offset, offset}, 1, ImGui::GetColorU32(ImGuiCol_Border));
    }

    static ImVec2 text_size(const string &text) {
        return ImGui::CalcTextSize(text.c_str());
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

enum IO_ {
    IO_None = -1,
    IO_In,
    IO_Out,
};

using IO = IO_;

string to_string(const IO direction) {
    switch (direction) {
        case IO_None: return "None";
        case IO_In: return "In";
        case IO_Out: return "Out";
    }
}

// An abstract block diagram schema
struct Schema {
    Tree tree;
    const Count in_count, out_count;
    const std::vector<Schema *> children{};
    const Count descendents = 0; // The number of boxes within this schema (recursively).
    const bool is_top_level;
    Tree parent;
    float w = 0, h = 0; // Populated in `place_size`
    float x = 0, y = 0; // Fields populated in `place`

    Orientation orientation = LeftRight;

    Schema(Tree t, Count in_count, Count out_count, std::vector<Schema *> children = {}, Count directDescendents = 0, Tree parent = nullptr)
        : tree(t), in_count(in_count), out_count(out_count), children(std::move(children)),
          descendents(directDescendents + ::ranges::accumulate(this->children | views::transform([](Schema *child) { return child->descendents; }), 0)),
          is_top_level(descendents >= FoldComplexity), parent(parent) {}

    virtual ~Schema() = default;

    inline Schema *child(Count i) const { return children[i]; }

    Count io_count(IO direction) const { return direction == IO_In ? in_count : out_count; };
    Count io_count(IO direction, const Count child_index) const { return child_index < children.size() ? children[child_index]->io_count(direction) : 0; };
    virtual ImVec2 point(IO direction, Count channel) const = 0;

    void place(const DeviceType type, float new_x, float new_y, Orientation new_orientation) {
        x = new_x;
        y = new_y;
        orientation = new_orientation;
        _place(type);
    }
    void place_size(const DeviceType type) {
        for (auto *child: children) child->place_size(type);
        _place_size(type);
    }
    void place(const DeviceType type) {
        _place(type);
    }
    void draw(Device &device) const {
        for (const auto *child: children) child->draw(device);
        _draw(device);
    };
    inline bool is_lr() const { return orientation == LeftRight; }
    inline float dir_unit() const { return is_lr() ? 1 : -1; }

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

    void draw_rect(Device &device) const {
        device.rect({x, y, w, h}, {.fill_color = "#00000000", .stroke_color = "#0000ff", .stroke_width = 1});
    }

    void draw_channel_labels(Device &device) const {
        for (const IO direction: {IO_In, IO_Out}) {
            for (Count channel = 0; channel < io_count(direction); channel++) {
                device.text(
                    point(direction, channel),
                    format("{}:{}", to_string(direction), channel),
                    {.color = "#0000ff", .justify = TextStyle::Justify::Right, .padding_right = 2, .padding_bottom = 3, .font_size = 8, .font_style = TextStyle::FontStyle::Bold, .top = true}
                );
                device.circle(point(direction, channel), 1.5, "#0000ff");
            }
            for (Count ci = 0; ci < children.size(); ci++) {
                for (Count channel = 0; channel < io_count(direction, ci); channel++) {
                    device.text(
                        child(ci)->point(direction, channel),
                        format("({})->{}:{}", ci, to_string(direction), channel),
                        {.color = "#ff0000", .justify = TextStyle::Justify::Right, .padding_right = 2, .font_size = 6, .font_style = TextStyle::FontStyle::Bold, .top = true}
                    );
                    device.circle(child(ci)->point(direction, channel), 1, "#ff0000");
                }
            }
        }
    }

    inline ImVec2 position() const { return {x, y}; }
    inline ImVec2 size() const { return {w, h}; }
    inline ImVec2 mid() const { return position() + size() / 2; }
    inline ImVec4 xywh() const { return {x, y, w, h}; }

    inline Schema *s1() const { return children[0]; }
    inline Schema *s2() const { return children[1]; }

protected:
    virtual void _place_size(DeviceType) = 0;
    virtual void _place(DeviceType) {};
    virtual void _draw(Device &) const {}
};

struct IOSchema : Schema {
    IOSchema(Tree t, Count in_count, Count out_count, std::vector<Schema *> children = {}, Count directDescendents = 0, Tree parent = nullptr)
        : Schema(t, in_count, out_count, std::move(children), directDescendents, parent) {}

    ImVec2 point(IO io, Count i) const override {
        const float dy = dir_unit() * WireGap;
        const float _y = mid().y - WireGap * float(io_count(io) - 1) / 2;
        return {x + ((io == IO_In && is_lr()) || (io == IO_Out && !is_lr()) ? 0 : w), _y + float(i) * dy};
    }
};

// A simple rectangular box with text and inputs/outputs.
struct BlockSchema : IOSchema {
    BlockSchema(Tree t, Count in_count, Count out_count, string text, string color, Schema *inner = nullptr)
        : IOSchema(t, in_count, out_count, {}, 1), text(std::move(text)), color(std::move(color)), inner(inner) {}

    void _place_size(const DeviceType type) override {
        const float text_w = (type == SVGDeviceType ? SVGDevice::text_size(text) : ImGuiDevice::text_size(text)).x;
        w = 2 * XGap + max(3 * WireGap, text_w);
        h = 2 * YGap + max(3 * WireGap, float(max(in_count, out_count)) * WireGap);
    }

    void _place(const DeviceType type) override {
        IOSchema::_place(type);
        if (inner) {
            inner->place_size(type);
            inner->place(type);
        }
    }

    void _draw(Device &device) const override {
        if (inner && device.type() == SVGDeviceType) inner->draw(device.type());
        const string &link = inner ? svgFileName(tree) : "";
        device.rect(xywh() + ImVec4{XGap, YGap, -2 * XGap, -2 * YGap}, color, link);
        device.text(mid(), text, {"#ffffff"}, link);

        // Draw a small point that indicates the first input (like an integrated circuits).
        device.dot(position() + (is_lr() ? ImVec2{XGap, YGap} : ImVec2{w - XGap, h - YGap}), orientation);
        draw_connections(device);
    }

    void draw_connections(Device &device) const {
        const ImVec2 d = {dir_unit() * XGap, 0};
        for (Count i = 0; i < io_count(IO_In); i++) {
            const auto &p = point(IO_In, i);
            device.line(p, p + d); // Input lines
            device.arrow(p + d, orientation); // Input arrows
        }
        for (Count i = 0; i < io_count(IO_Out); i++) {
            const auto &p = point(IO_Out, i);
            device.line(p - d, p); // Output lines
        }
    }

    const string text, color;
    Schema *inner;
};

// Simple cables (identity box) in parallel.
struct CableSchema : Schema {
    CableSchema(Tree t, Count n = 1) : Schema(t, n, n) {}

    // The width of a cable is null, so its input and output connection points are the same.
    void _place_size(const DeviceType) override {
        w = 0;
        h = float(in_count) * WireGap;
    }

    // Place the communication points vertically spaced by `WireGap`.
    void _place(const DeviceType) override {
        for (Count i = 0; i < in_count; i++) {
            const float dx = WireGap * (float(i) + 0.5f);
            points[i] = {x, y + (is_lr() ? dx : h - dx)};
        }
    }

    ImVec2 point(IO io, Count i) const override { return points[i]; }

private:
    std::vector<ImVec2> points{in_count};
};

// An inverter is a circle followed by a triangle.
// It corresponds to '*(-1)', and it's used to create more compact diagrams.
struct InverterSchema : BlockSchema {
    InverterSchema(Tree t) : BlockSchema(t, 1, 1, "-1", InverterColor) {}

    void _place_size(const DeviceType) override {
        w = 2.5f * WireGap;
        h = WireGap;
    }

    void _draw(Device &device) const override {
        const float x1 = w - 2 * XGap;
        const float y1 = 0.5f + (h - 1) / 2;
        const auto tri_a = position() + ImVec2{XGap + (is_lr() ? 0 : x1), 0};
        const auto tri_b = tri_a + ImVec2{(is_lr() ? x1 - 2 * InverterRadius : 2 * InverterRadius - x1 - x), y1};
        const auto tri_c = tri_a + ImVec2{0, h - 1};
        device.circle(tri_b + ImVec2{dir_unit() * InverterRadius, 0}, InverterRadius, color);
        device.triangle(tri_a, tri_b, tri_c, color);
        draw_connections(device);
    }
};

// Cable termination
struct CutSchema : Schema {
    // A Cut is represented by a small black dot.
    // It has 1 input and no out_count.
    CutSchema(Tree t) : Schema(t, 1, 0) {}

    // 0 width and 1 height, for the wire.
    void _place_size(const DeviceType) override {
        w = 0;
        h = 1;
    }

    // A cut is represented by a small black dot.
    void _draw(Device &) const override {
        // device.circle(point, WireGap / 8);
    }

    // A Cut has only one input point
    ImVec2 point(IO io, Count) const override {
        fgassert(io == IO_In);
        return {x, mid().y};
    }
};

struct ParallelSchema : Schema {
    ParallelSchema(Tree t, Schema *s1, Schema *s2)
        : Schema(t, s1->in_count + s2->in_count, s1->out_count + s2->out_count, {s1, s2}) {}

    void _place_size(const DeviceType) override {
        w = max(s1()->w, s2()->w);
        h = s1()->h + s2()->h;
    }
    void _place(const DeviceType type) override {
        auto *top = children[is_lr() ? 0 : 1];
        auto *bottom = children[is_lr() ? 1 : 0];
        top->place(type, x + (w - top->w) / 2, y, orientation);
        bottom->place(type, x + (w - bottom->w) / 2, y + top->h, orientation);
    }

    void _draw(Device &device) const override {
        for (Count i = 0; i < in_count; i++) device.line(point(IO_In, i), i < io_count(IO_In, 0) ? child(0)->point(IO_In, i) : child(1)->point(IO_In, i - io_count(IO_In, 0)));
        for (Count i = 0; i < out_count; i++) device.line(i < io_count(IO_Out, 0) ? child(0)->point(IO_Out, i) : child(1)->point(IO_Out, i - io_count(IO_Out, 0)), point(IO_Out, i));
    }

    ImVec2 point(IO io, Count i) const override {
        const float d = io == IO_In ? -1 : 1;
        return i < io_count(io, 0) ?
               child(0)->point(io, i) + ImVec2{d * dir_unit() * (w - s1()->w) / 2, 0} :
               child(1)->point(io, i - io_count(io, 0)) + ImVec2{d * dir_unit() * (w - s2()->w) / 2, 0};
    }
};

// Place and connect two diagrams in recursive composition
struct RecursiveSchema : Schema {
    RecursiveSchema(Tree t, Schema *s1, Schema *s2) : Schema(t, s1->in_count - s2->out_count, s1->out_count, {s1, s2}) {
        fgassert(s1->in_count >= s2->out_count);
        fgassert(s1->out_count >= s2->in_count);
    }

    void _place_size(const DeviceType) override {
        w = max(s1()->w, s2()->w) + 2 * WireGap * float(max(io_count(IO_In, 1), io_count(IO_Out, 1)));
        h = s1()->h + s2()->h;
    }

    // The two schemas are centered vertically, stacked on top of each other, with stacking order dependent on orientation.
    void _place(const DeviceType type) override {
        auto *top_schema = children[is_lr() ? 1 : 0];
        auto *bottom_schema = children[is_lr() ? 0 : 1];
        top_schema->place(type, x + (w - top_schema->w) / 2, y, RightLeft);
        bottom_schema->place(type, x + (w - bottom_schema->w) / 2, y + top_schema->h, LeftRight);
    }

    void _draw(Device &device) const override {
        const float dw = dir_unit() * WireGap;
        // Out0->In1 feedback connections
        for (Count i = 0; i < io_count(IO_In, 1); i++) {
            const auto &in1 = child(1)->point(IO_In, i);
            const auto &out0 = child(0)->point(IO_Out, i);
            const auto &from = ImVec2{is_lr() ? max(in1.x, out0.x) : min(in1.x, out0.x), out0.y} + ImVec2{float(i) * dw, 0};
            // Draw the delay sign of a feedback connection (three sides of a square centered around the feedback source point).
            const auto &corner1 = from - ImVec2{dw / 4, dw / 2};
            const auto &corner2 = from + ImVec2{dw / 4, -dw / 2};
            device.line(from - ImVec2{dw / 4, 0}, corner1);
            device.line(corner1, corner2);
            device.line(corner2, from + ImVec2{dw / 4, 0});
            // Draw the feedback line
            const ImVec2 &bend = {from.x, in1.y};
            device.line({from.x, from.y - dw / 2}, bend);
            device.line(bend, in1);
        }
        // Non-recursive output lines
        for (Count i = 0; i < out_count; i++) device.line(child(0)->point(IO_Out, i), point(IO_Out, i));
        // Input lines
        for (Count i = 0; i < in_count; i++) device.line(point(IO_In, i), child(0)->point(IO_In, i + s2()->out_count));
        // Out1->In0 feedfront connections
        for (Count i = 0; i < io_count(IO_Out, 1); i++) {
            const auto &from = child(1)->point(IO_Out, i);
            const auto &from_dx = from - ImVec2{dw * float(i), 0};
            const auto &to = child(0)->point(IO_In, i);
            const ImVec2 &corner1 = {to.x, from_dx.y};
            const ImVec2 &corner2 = {from_dx.x, to.y};
            const ImVec2 &bend = is_lr() ? (from_dx.x > to.x ? corner1 : corner2) : (from_dx.x > to.x ? corner2 : corner1);
            device.line(from, from_dx);
            device.line(from_dx, bend);
            device.line(bend, to);
        }
    }

    ImVec2 point(IO io, Count i) const override {
        const bool lr = (io == IO_In && is_lr()) || (io == IO_Out && !is_lr());
        return {lr ? x : x + w, child(0)->point(io, i + (io == IO_In ? io_count(IO_Out, 1) : 0)).y};
    }
};

struct BinarySchema : Schema {
    BinarySchema(Tree t, Schema *s1, Schema *s2) : Schema(t, s1->in_count, s2->out_count, {s1, s2}) {}

    ImVec2 point(IO io, Count i) const override { return child(io == IO_In ? 0 : 1)->point(io, i); }

    void _place_size(const DeviceType) override {
        w = s1()->w + s2()->w + horizontal_gap();
        h = max(s1()->h, s2()->h);
    }

    // Place the two components horizontally, centered, with enough space for the connections.
    void _place(const DeviceType type) override {
        const float horz_gap = horizontal_gap();
        auto *left = children[is_lr() ? 0 : 1];
        auto *right = children[is_lr() ? 1 : 0];
        left->place(type, x, y + max(0.0f, right->h - left->h) / 2, orientation);
        right->place(type, x + left->w + horz_gap, y + max(0.0f, left->h - right->h) / 2, orientation);
    }

    virtual float horizontal_gap() const {
        return (s1()->h + s2()->h) * BinarySchemaHorizontalGapRatio;
    }
};

struct SequentialSchema : BinarySchema {
    // The components s1 and s2 must be "compatible" (s1: n->m and s2: m->q).
    SequentialSchema(Tree t, Schema *s1, Schema *s2) : BinarySchema(t, s1, s2) {
        fgassert(s1->out_count == s2->in_count);
    }

    void _place_size(const DeviceType type) override {
        if (s1()->x == 0 && s1()->y == 0 && s2()->x == 0 && s2()->y == 0) {
            s1()->place(type, 0, max(0.0f, s2()->h - s1()->h) / 2, LeftRight);
            s2()->place(type, 0, max(0.0f, s1()->h - s2()->h) / 2, LeftRight);
        }
        BinarySchema::_place_size(type);
    }

    void _place(const DeviceType type) override {
        BinarySchema::_place(type);
        channels_for_direction = {};
        for (Count i = 0; i < io_count(IO_Out, 0); i++) {
            const auto dy = child(1)->point(IO_In, i).y - child(0)->point(IO_Out, i).y;
            channels_for_direction[dy == 0 ? ImGuiDir_None : dy < 0 ? ImGuiDir_Up : ImGuiDir_Down].emplace_back(i);
        }
    }

    void _draw(Device &device) const override {
        if (!SequentialConnectionZigzag) {
            // Draw a straight, potentially diagonal cable.
            for (Count i = 0; i < io_count(IO_Out, 0); i++) device.line(child(0)->point(IO_Out, i), child(1)->point(IO_In, i));
            return;
        }
        // Draw upward zigzag cables, with the x turning point determined by the index of the connection in the group.
        for (const auto dir: views::keys(channels_for_direction)) {
            const auto &channels = channels_for_direction.at(dir);
            for (Count i = 0; i < channels.size(); i++) {
                const auto channel = channels[i];
                const auto from = child(0)->point(IO_Out, channel);
                const auto to = child(1)->point(IO_In, channel);
                if (dir == ImGuiDir_None) {
                    device.line(from, to); // Draw a  straight cable
                } else {
                    const bool lr = from.x < to.x;
                    const Count x_position = lr ? i : channels.size() - i - 1;
                    const float bend_x = from.x + float(x_position) * (lr ? WireGap : -WireGap);
                    device.line(from, {bend_x, from.y});
                    device.line({bend_x, from.y}, {bend_x, to.y});
                    device.line({bend_x, to.y}, to);
                }
            }
        }
    }

    // Compute the horizontal gap needed to draw the internal wires.
    // It depends on the largest group of connections that go in the same up/down direction.
    float horizontal_gap() const override {
        if (io_count(IO_Out, 0) == 0) return 0;

        ImGuiDir prev_dir = ImGuiDir_None;
        Count size = 0;
        std::map<ImGuiDir, Count> MaxGroupSize; // Store the size of the largest group for each direction.
        for (Count i = 0; i < io_count(IO_Out, 0); i++) {
            const float yd = child(1)->point(IO_In, i).y - child(0)->point(IO_Out, i).y;
            const auto dir = yd < 0 ? ImGuiDir_Up : yd > 0 ? ImGuiDir_Down : ImGuiDir_None;
            size = dir == prev_dir ? size + 1 : 1;
            prev_dir = dir;
            MaxGroupSize[dir] = max(MaxGroupSize[dir], size);
        }

        return WireGap * float(max(MaxGroupSize[ImGuiDir_Up], MaxGroupSize[ImGuiDir_Down]));
    }

private:
    std::map<ImGuiDir, std::vector<Count>> channels_for_direction;
};

// Place and connect two diagrams in merge composition.
// The outputs of the first schema are merged to the inputs of the second.
struct MergeSchema : BinarySchema {
    MergeSchema(Tree t, Schema *s1, Schema *s2) : BinarySchema(t, s1, s2) {}

    void _draw(Device &device) const override {
        for (Count i = 0; i < io_count(IO_Out, 0); i++) device.line(child(0)->point(IO_Out, i), child(1)->point(IO_In, i % io_count(IO_In, 1)));
    }
};

// Place and connect two diagrams in split composition.
// The outputs the first schema are distributed to the inputs of the second.
struct SplitSchema : BinarySchema {
    SplitSchema(Tree t, Schema *s1, Schema *s2) : BinarySchema(t, s1, s2) {}

    void _draw(Device &device) const override {
        for (Count i = 0; i < io_count(IO_In, 1); i++) device.line(child(0)->point(IO_Out, i % io_count(IO_Out, 0)), child(1)->point(IO_In, i));
    }
};

Schema *make_sequential(Tree t, Schema *s1, Schema *s2) {
    const auto o = s1->out_count;
    const auto i = s2->in_count;
    return new SequentialSchema(t,
        o < i ? new ParallelSchema(t, s1, new CableSchema(t, i - o)) : s1,
        o > i ? new ParallelSchema(t, s2, new CableSchema(t, o - i)) : s2
    );
}

// A `DecorateSchema` is a schema surrounded by a dashed rectangle with a label on the top left, and arrows added to the outputs.
// If the number of boxes inside is over the `box_complexity` threshold, add additional padding and draw output arrows.
struct DecorateSchema : IOSchema {
    DecorateSchema(Tree t, Schema *inner, string text, Tree parent = nullptr)
        : IOSchema(t, inner->in_count, inner->out_count, {inner}, 0, parent), text(std::move(text)) {}

    void _place_size(const DeviceType) override {
        w = s1()->w + 2 * (DecorateSchemaMargin + (s1()->is_top_level ? TopSchemaMargin : 0));
        h = s1()->h + 2 * (DecorateSchemaMargin + (s1()->is_top_level ? TopSchemaMargin : 0));
    }

    void _place(const DeviceType type) override {
        const float margin = DecorateSchemaMargin + (is_top_level ? TopSchemaMargin : 0);
        s1()->place(type, x + margin, y + margin, orientation);
    }

    void _draw(Device &device) const override {
        const float top_level_margin = is_top_level ? TopSchemaMargin : 0;
        const float margin = 2 * top_level_margin + DecorateSchemaMargin;
        const auto rect_pos = position() + ImVec2{margin, margin} / 2;
        const auto rect_size = size() - ImVec2{margin, margin};
        device.grouprect({rect_pos.x, rect_pos.y, rect_size.x, rect_size.y}, text);
        for (Count i = 0; i < in_count; i++) device.line(point(IO_In, i), child(0)->point(IO_In, i));
        for (Count i = 0; i < out_count; i++) {
            device.line(child(0)->point(IO_Out, i), point(IO_Out, i));
            if (is_top_level) device.arrow(point(IO_Out, i), orientation);
        }
    }

    ImVec2 point(IO io, Count i) const override {
        const float d = io == IO_In ? -1 : 1;
        return child(0)->point(io, i) + ImVec2{dir_unit() * d * TopSchemaMargin, 0};
    }

private:
    string text;
};

struct RouteSchema : IOSchema {
    RouteSchema(Tree t, Count in_count, Count out_count, std::vector<int> routes)
        : IOSchema(t, in_count, out_count), color("#EEEEAA"), routes(std::move(routes)) {}

    void _place_size(const DeviceType) override {
        const float minimal = 3 * WireGap;
        h = 2 * YGap + max(minimal, max(float(in_count), float(out_count)) * WireGap);
        w = 2 * XGap + max(minimal, h * 0.75f);
    }

    void _draw(Device &device) const override {
        if (DrawRouteFrame) {
            device.rect(xywh() + ImVec4{XGap, YGap, -2 * XGap, -2 * YGap}, color, "");
            // Draw the orientation mark, a small point that indicates the first input (like integrated circuits).
            device.dot(position() + (is_lr() ? ImVec2{XGap, YGap} : ImVec2{w - XGap, h - YGap}), orientation);
            // Input arrows
            for (Count i = 0; i < io_count(IO_In); i++) device.arrow(point(IO_In, i) + ImVec2{dir_unit() * XGap, 0}, orientation);
        }

        // Input/output & route wires
        const auto d = ImVec2{dir_unit() * XGap, 0};
        for (const IO io: {IO_In, IO_Out}) {
            for (Count i = 0; i < io_count(io); i++) {
                const auto &p = point(io, i);
                device.line(io == IO_In ? p : p - d, io == IO_In ? p + d : p);
            }
        }
        for (Count i = 0; i < routes.size() - 1; i += 2) {
            const Count src = routes[i];
            const Count dst = routes[i + 1];
            if (src > 0 && src <= in_count && dst > 0 && dst <= out_count) {
                device.line(point(IO_In, src - 1) + d, point(IO_Out, dst - 1) - d);
            }
        }
    }

protected:
    const string color;
    const std::vector<int> routes; // Route description: s1,d2,s2,d2,...
};

static bool isBoxBinary(Tree t, Tree &x, Tree &y) {
    return isBoxPar(t, x, y) || isBoxSeq(t, x, y) || isBoxSplit(t, x, y) || isBoxMerge(t, x, y) || isBoxRec(t, x, y);
}

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

static string print_tree(Tree tree) {
    stringstream ss;
    ss << boxpp(tree);
    return ss.str();
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

    throw std::runtime_error("Not a valid list of numbers : " + print_tree(t));
}

// Convert user interface element into a textual representation
static string userInterfaceDescription(Tree box) {
    Tree t1, label, cur, min, max, step, chan;
    if (isBoxButton(box, label)) return "button(" + extractName(label) + ')';
    if (isBoxCheckbox(box, label)) return "checkbox(" + extractName(label) + ')';
    if (isBoxVSlider(box, label, cur, min, max, step)) return "vslider(" + extractName(label) + ", " + print_tree(cur) + ", " + print_tree(min) + ", " + print_tree(max) + ", " + print_tree(step) + ')';
    if (isBoxHSlider(box, label, cur, min, max, step)) return "hslider(" + extractName(label) + ", " + print_tree(cur) + ", " + print_tree(min) + ", " + print_tree(max) + ", " + print_tree(step) + ')';
    if (isBoxVGroup(box, label, t1)) return "vgroup(" + extractName(label) + ", " + print_tree(t1) + ')';
    if (isBoxHGroup(box, label, t1)) return "hgroup(" + extractName(label) + ", " + print_tree(t1) + ')';
    if (isBoxTGroup(box, label, t1)) return "tgroup(" + extractName(label) + ", " + print_tree(t1) + ')';
    if (isBoxHBargraph(box, label, min, max)) return "hbargraph(" + extractName(label) + ", " + print_tree(min) + ", " + print_tree(max) + ')';
    if (isBoxVBargraph(box, label, min, max)) return "vbargraph(" + extractName(label) + ", " + print_tree(min) + ", " + print_tree(max) + ')';
    if (isBoxNumEntry(box, label, cur, min, max, step)) return "nentry(" + extractName(label) + ", " + print_tree(cur) + ", " + print_tree(min) + ", " + print_tree(max) + ", " + print_tree(step) + ')';
    if (isBoxSoundfile(box, label, chan)) return "soundfile(" + extractName(label) + ", " + print_tree(chan) + ')';

    throw std::runtime_error("ERROR : unknown user interface element");
}

static Schema *Tree2Schema(Tree t, bool allow_links = true);

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
    if (isBoxPar(t, a, b)) return new ParallelSchema(t, Tree2Schema(a), Tree2Schema(b));
    if (isBoxSplit(t, a, b)) return new SplitSchema(t, Tree2Schema(a), Tree2Schema(b));
    if (isBoxMerge(t, a, b)) return new MergeSchema(t, Tree2Schema(a), Tree2Schema(b));
    if (isBoxRec(t, a, b)) return new RecursiveSchema(t, Tree2Schema(a), Tree2Schema(b));

    if (isBoxSlot(t, &i)) return new BlockSchema(t, 0, 1, getTreeName(t), SlotColor);

    if (isBoxSymbolic(t, a, b)) {
        // Generate an abstraction schema by placing in sequence the input slots and the body.
        auto *input_slots = make_input_slot(a);
        Tree _a, _b;
        while (isBoxSymbolic(b, _a, _b)) {
            input_slots = new ParallelSchema(b, input_slots, make_input_slot(_a));
            b = _b;
        }
        auto *abstraction = make_sequential(b, input_slots, Tree2Schema(b));
        return getTreeName(t) ? abstraction : new DecorateSchema(t, abstraction, "Abstraction");
    }
    if (isBoxEnvironment(t)) return new BlockSchema(t, 0, 0, "environment{...}", NormalColor);

    Tree c;
    if (isBoxRoute(t, a, b, c)) {
        int ins, outs;
        vector<int> route;
        // Build n x m cable routing
        if (isBoxInt(a, &ins) && isBoxInt(b, &outs) && isIntTree(c, route)) return new RouteSchema(t, ins, outs, route);

        throw std::runtime_error("Invalid route expression : " + print_tree(t));
    }

    throw std::runtime_error("ERROR in Tree2SchemaNode, box expression not recognized: " + print_tree(t));
}

static bool AllowSchemaLinks = true; // Set to `false` to draw all schemas inline in one big diagram. Set to `true` to split into files (for SVG rendering).
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
// todo show tree to a given level
static Schema *Tree2Schema(Tree t, bool allow_links) {
    static std::stack<Tree> treeFocusHierarchy; // As we descend into the tree, keep track of ancestors for backlinks.
    if (const char *name = getTreeName(t)) {
        Tree parent = treeFocusHierarchy.empty() ? nullptr : treeFocusHierarchy.top();
        treeFocusHierarchy.push(t);
        auto *schema = new DecorateSchema{t, Tree2SchemaNode(t), name, parent};
        treeFocusHierarchy.pop();
        if (schema->is_top_level && AllowSchemaLinks && allow_links) {
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
    IsTreePureRouting.clear();
    if (box) {
        // Render SVG diagram(s)
        fs::remove_all(FaustDiagramsPath);
        fs::create_directory(FaustDiagramsPath);
        auto *svg_schema = Tree2Schema(box, false); // Ensure top-level is not compressed into a link.
        svg_schema->place_size(SVGDeviceType);
        svg_schema->place(SVGDeviceType);
        svg_schema->draw(SVGDeviceType);

        // Render ImGui diagram
        active_schema = Tree2Schema(box, false); // Ensure top-level is not compressed into a link.
        active_schema->place_size(ImGuiDeviceType);
        active_schema->place(ImGuiDeviceType);
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
