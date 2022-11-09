#include <sstream>
#include <map>
#include <stack>

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/take_while.hpp>

#include "faust/dsp/libfaust-signal.h"
#include "faust/dsp/libfaust-box.h"

#include "../../App.h"
#include "../../Helper/basen.h"

//-----------------------------------------------------------------------------
// [SECTION] Diagram
//-----------------------------------------------------------------------------

using Count = unsigned int;
enum DeviceType { DeviceType_ImGui, DeviceType_SVG };
enum Diagram { DiagramForward, DiagramReverse };

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

    const ImVec4 color{1, 1, 1, 1};
    const Justify justify{Middle};
    const float padding_right{0};
    const float padding_bottom{0};
    const float scale_height{1}; // todo remove this in favor of using a set (two for now) of predetermined font sizes
    const FontStyle font_style{Normal};
};

struct RectStyle {
    const ImVec4 &fill_color{1, 1, 1, 1};
    const ImVec4 &stroke_color{0, 0, 0, 0};
    const float stroke_width{0};
    const float corner_radius{0};
};

static inline ImVec2 scale(const ImVec2 &p);
static inline ImRect scale(const ImRect &r);
static inline float scale(float f);
static inline ImVec2 get_scale();

static inline ImGuiDir global_direction(Diagram orientation) {
    const ImGuiDir dir = s.Style.FlowGrid.DiagramDirection;
    return (dir == ImGuiDir_Right && orientation == DiagramForward) || (dir == ImGuiDir_Left && orientation == DiagramReverse) ?
           ImGuiDir_Right : ImGuiDir_Left;
}

static inline bool is_lr(Diagram orientation) { return global_direction(orientation) == ImGuiDir_Right; }

// Device accepts unscaled, un-offset positions, and takes care of scaling/offsetting internally.
struct Device {
    static constexpr float DecorateLabelOffset = 14; // Not configurable, since it's a pain to deal with right.
    static constexpr float DecorateLabelXPadding = 3;

    Device(const ImVec2 &offset = {0, 0}) : offset(offset) {}
    virtual ~Device() = default;
    virtual DeviceType type() = 0;
    virtual void rect(const ImRect &rect, const RectStyle &style) = 0;
    virtual void grouprect(const ImRect &rect, const string &text) = 0; // A labeled grouping
    virtual void triangle(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec4 &color) = 0;
    virtual void circle(const ImVec2 &pos, float radius, const ImVec4 &fill_color, const ImVec4 &stroke_color) = 0;
    virtual void arrow(const ImVec2 &pos, Diagram orientation) = 0;
    virtual void line(const ImVec2 &start, const ImVec2 &end) = 0;
    virtual void text(const ImVec2 &pos, const string &text, const TextStyle &style) = 0;
    virtual void dot(const ImVec2 &pos, const ImVec4 &fill_color) = 0;

    ImVec2 at(const ImVec2 &p) const { return offset + scale(p); }

    ImVec2 offset{};
};

using namespace ImGui;

// ImGui saves font name as "{Name}.{Ext}, {Size}px"
static inline string get_font_name() {
    const string name = GetFont()->GetDebugName();
    return name.substr(0, name.find_first_of('.'));
}
static inline string get_font_path() {
    const string name = GetFont()->GetDebugName();
    return format("../res/fonts/{}", name.substr(0, name.find_first_of(','))); // Path is relative to build dir.
}
static inline string get_font_base64() {
    static map<string, string> base64_for_font_name; // avoid recomputing
    const string &font_name = get_font_name();
    if (!base64_for_font_name.contains(font_name)) {
        const string ttf_contents = FileIO::read(get_font_path());
        string ttf_base64;
        bn::encode_b64(ttf_contents.begin(), ttf_contents.end(), back_inserter(ttf_base64));
        base64_for_font_name[font_name] = ttf_base64;
    }
    return base64_for_font_name.at(font_name);
}

static ImVec2 text_size(const string &text) { return CalcTextSize(text.c_str()); }

struct SVGDevice : Device {
    SVGDevice(fs::path directory, string file_name, ImVec2 size) : directory(std::move(directory)), file_name(std::move(file_name)) {
        const auto &[w, h] = scale(size);
        stream << format(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {} {}")", w, h);
        stream << (s.Style.FlowGrid.DiagramScaleFill ? R"( width="100%" height="100%">)" : format(R"( width="{}" height="{}">)", w, h));

        // Embed the current font as a base64-encoded string.
        stream << format(R"(
        <defs><style>
            @font-face{{
                font-family:"{}";
                src:url(data:application/font-woff;charset=utf-8;base64,{}) format("woff");
                font-weight:normal;font-style:normal;
            }}
        </style></defs>)", get_font_name(), get_font_base64());
    }

    ~SVGDevice() override {
        stream << "</svg>\n";
        FileIO::write(directory / file_name, stream.str());
    }

    DeviceType type() override { return DeviceType_SVG; }

    static string xml_sanitize(const string &name) {
        static map<char, string> replacements{{'<', "&lt;"}, {'>', "&gt;"}, {'\'', "&apos;"}, {'"', "&quot;"}, {'&', "&amp;"}};

        auto replaced_name = name;
        for (const auto &[ch, replacement]: replacements) {
            replaced_name = replace(replaced_name, ch, replacement);
        }
        return replaced_name;
    }

    void rect(const ImRect &r, const RectStyle &style) override {
        const auto &sr = scale(r);
        const auto &[fill_color, stroke_color, stroke_width, corner_radius] = style;
        stream << format(R"(<rect x="{}" y="{}" width="{}" height="{}" rx="{}" style="stroke:{};stroke-width={};fill:{};"/>)",
            sr.Min.x, sr.Min.y, sr.GetWidth(), sr.GetHeight(), corner_radius, rgb_color(stroke_color), stroke_width, rgb_color(fill_color));
    }

    // Only SVG device has a rect-with-link method
    void rect(const ImRect &r, const RectStyle &style, const string &link) {
        if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link));
        rect(r, style);
        if (!link.empty()) stream << "</a>";
    }

    void grouprect(const ImRect &r, const string &text) override {
        const auto &sr = scale(r);
        const auto &tl = sr.Min;
        const auto &tr = sr.GetTR();
        const float text_x = tl.x + scale(DecorateLabelOffset);
        const auto &padding = scale({DecorateLabelXPadding, 0});
        const ImVec2 &text_right = {min(text_x + scale(text_size(text)).x + padding.x, tr.x), tr.y};
        const auto &label_color = s.Style.FlowGrid.Colors[FlowGridCol_DiagramGroupTitle];
        const auto &stroke_color = s.Style.FlowGrid.Colors[FlowGridCol_DiagramGroupStroke];
        const float rad = scale(s.Style.FlowGrid.DiagramDecorateCornerRadius);
        const float line_width = scale(s.Style.FlowGrid.DiagramDecorateLineWidth);
        // Going counter-clockwise instead of clockwise, like in the ImGui implementation, since that's what paths expect for corner rounding to work.
        stream << format(R"(<path d="m{},{} h{} a{},{} 0 00 {},{} v{} a{},{} 0 00 {},{} h{} a{},{} 0 00 {},{} v{} a{},{} 0 00 {},{} h{}" stroke-width="{}" stroke="{}" fill="none"/>)",
            text_x - padding.x, tl.y, -scale(DecorateLabelOffset) + padding.x + rad, rad, rad, -rad, rad, // before text to top-left
            (sr.GetHeight() - 2 * rad), rad, rad, rad, rad, // top-left to bottom-left
            (sr.GetWidth() - 2 * rad), rad, rad, rad, -rad, // bottom-left to bottom-right
            -(sr.GetHeight() - 2 * rad), rad, rad, -rad, -rad, // bottom-right to top-right
            -(tr.x - rad - text_right.x), // top-right to after text
            line_width, rgb_color(stroke_color));
        stream << format(R"(<text x="{}" y="{}" font-family="{}" font-size="{}" fill="{}" dominant-baseline="middle">{}</text>)",
            text_x, tl.y, get_font_name(), get_font_size(), rgb_color(label_color), xml_sanitize(text));
    }

    void triangle(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec4 &color) override {
        stream << get_triangle(at(p1), at(p2), at(p3), {0, 0, 0, 0}, color);
    }

    void circle(const ImVec2 &pos, float radius, const ImVec4 &fill_color, const ImVec4 &stroke_color) override {
        const auto [x, y] = at(pos);
        stream << format(R"(<circle fill="{}" stroke="{}" stroke-width=".5" cx="{}" cy="{}" r="{}"/>)",
            rgb_color(fill_color), rgb_color(stroke_color), x, y, radius);
    }

    void arrow(const ImVec2 &pos, Diagram orientation) override {
        stream << arrow_pointing_at(at(pos), scale(s.Style.FlowGrid.DiagramArrowSize), orientation, s.Style.FlowGrid.Colors[FlowGridCol_DiagramLine]);
    }

    void line(const ImVec2 &start, const ImVec2 &end) override {
        const string line_cap = start.x == end.x || start.y == end.y ? "butt" : "round";
        const auto &start_scaled = at(start);
        const auto &end_scaled = at(end);
        const auto &color = s.Style.FlowGrid.Colors[FlowGridCol_DiagramLine];
        const auto width = scale(s.Style.FlowGrid.DiagramWireWidth);
        stream << format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}"  style="stroke:{}; stroke-linecap:{}; stroke-width:{};"/>)",
            start_scaled.x, start_scaled.y, end_scaled.x, end_scaled.y, rgb_color(color), line_cap, width);
    }

    void text(const ImVec2 &pos, const string &text, const TextStyle &style) override {
        const auto &[color, justify, padding_right, padding_bottom, scale_height, font_style] = style;
        const string anchor = justify == TextStyle::Left ? "start" : justify == TextStyle::Middle ? "middle" : "end";
        const string font_style_formatted = font_style == TextStyle::FontStyle::Italic ? "italic" : "normal";
        const string font_weight = font_style == TextStyle::FontStyle::Bold ? "bold" : "normal";
        const auto &p = at(pos - ImVec2{padding_right, padding_bottom});
        stream << format(R"(<text x="{}" y="{}" font-family="{}" font-style="{}" font-weight="{}" font-size="{}" text-anchor="{}" fill="{}" dominant-baseline="middle">{}</text>)",
            p.x, p.y, get_font_name(), font_style_formatted, font_weight, get_font_size(), anchor, rgb_color(color), xml_sanitize(text));
    }

    // Only SVG device has a text-with-link method
    void text(const ImVec2 &pos, const string &str, const TextStyle &style, const string &link) {
        if (!link.empty()) stream << format(R"(<a href="{}">)", xml_sanitize(link));
        text(pos, str, style);
        if (!link.empty()) stream << "</a>";
    }

    void dot(const ImVec2 &pos, const ImVec4 &fill_color) override {
        const auto &p = at(pos);
        const float radius = scale(s.Style.FlowGrid.DiagramOrientationMarkRadius);
        stream << format(R"(<circle cx="{}" cy="{}" r="{}" fill="{}"/>)", p.x, p.y, radius, rgb_color(fill_color));
    }

    // Render an arrow. 'pos' is position of the arrow tip. half_sz.x is length from base to tip. half_sz.y is length on each side.
    static string arrow_pointing_at(const ImVec2 &pos, ImVec2 half_sz, Diagram orientation, const ImVec4 &color) {
        const float d = is_lr(orientation) ? -1 : 1;
        return get_triangle(ImVec2{pos.x + d * half_sz.x, pos.y - d * half_sz.y}, ImVec2{pos.x + d * half_sz.x, pos.y + d * half_sz.y}, pos, color, color);
    }

    static string get_triangle(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec4 &fill_color, const ImVec4 &stroke_color) {
        return format(R"(<polygon fill="{}" stroke="{}" stroke-width=".5" points="{},{} {},{} {},{}"/>)",
            rgb_color(fill_color), rgb_color(stroke_color), p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
    }

    static string rgb_color(const ImVec4 &color) {
        return format("rgb({}, {}, {}, {})", color.x * 255, color.y * 255, color.z * 255, color.w * 255);
    }

    // Scale factor to convert between ImGui font pixel height and SVG `font-size` attr value.
    // Determined empirically to make the two renderings look the same.
    static float get_font_size() { return scale(GetTextLineHeight()) * 0.8f; }

    fs::path directory;
    string file_name;

private:
    std::stringstream stream;
};

struct ImGuiDevice : Device {
    ImGuiDevice() : Device(GetCursorScreenPos()), draw_list(GetWindowDrawList()) {}

    DeviceType type() override { return DeviceType_ImGui; }

    void rect(const ImRect &rect, const RectStyle &style) override {
        const auto &[fill_color, stroke_color, stroke_width, corner_radius] = style;
        if (fill_color.w != 0) draw_list->AddRectFilled(at(rect.Min), at(rect.Max), ColorConvertFloat4ToU32(fill_color), corner_radius);
        if (stroke_color.w != 0) draw_list->AddRect(at(rect.Min), at(rect.Max), ColorConvertFloat4ToU32(stroke_color), corner_radius);
    }

    void grouprect(const ImRect &rect, const string &text) override {
        const auto &a = at(rect.Min);
        const auto &b = at(rect.Max);
        const auto &text_top_left = at(rect.Min + ImVec2{DecorateLabelOffset, 0});
        const auto &stroke_color = ColorConvertFloat4ToU32(s.Style.FlowGrid.Colors[FlowGridCol_DiagramGroupStroke]);
        const auto &label_color = ColorConvertFloat4ToU32(s.Style.FlowGrid.Colors[FlowGridCol_DiagramGroupTitle]);

        // Decorate a potentially rounded outline rect with a break in the top-left (to the right of max rounding) for the label text
        const float rad = scale(s.Style.FlowGrid.DiagramDecorateCornerRadius);
        const float line_width = scale(s.Style.FlowGrid.DiagramDecorateLineWidth);
        if (line_width > 0) {
            const auto &padding = scale({DecorateLabelXPadding, 0});
            draw_list->PathLineTo(text_top_left + ImVec2{text_size(text).x, 0} + padding);
            if (rad < 0.5f) {
                draw_list->PathLineTo({b.x, a.y});
                draw_list->PathLineTo(b);
                draw_list->PathLineTo({a.x, b.y});
                draw_list->PathLineTo(a);
            } else {
                draw_list->PathArcToFast({b.x - rad, a.y + rad}, rad, 9, 12);
                draw_list->PathArcToFast({b.x - rad, b.y - rad}, rad, 0, 3);
                draw_list->PathArcToFast({a.x + rad, b.y - rad}, rad, 3, 6);
                draw_list->PathArcToFast({a.x + rad, a.y + rad}, rad, 6, 9);
            }
            draw_list->PathLineTo(text_top_left - padding);
            draw_list->PathStroke(stroke_color, ImDrawFlags_None, line_width);
        }
        draw_list->AddText(text_top_left - ImVec2{0, GetFontSize() / 2}, label_color, text.c_str());
    }

    void triangle(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImVec4 &color) override {
        draw_list->AddTriangle(at(p1), at(p2), at(p3), ColorConvertFloat4ToU32(color));
    }

    void circle(const ImVec2 &p, float radius, const ImVec4 &fill_color, const ImVec4 &stroke_color) override {
        if (fill_color.w != 0) draw_list->AddCircleFilled(at(p), scale(radius), ColorConvertFloat4ToU32(fill_color));
        if (stroke_color.w != 0) draw_list->AddCircle(at(p), scale(radius), ColorConvertFloat4ToU32(stroke_color));
    }

    void arrow(const ImVec2 &p, Diagram orientation) override {
        RenderArrowPointingAt(draw_list,
            at(p) + ImVec2{0, 0.5f},
            scale(s.Style.FlowGrid.DiagramArrowSize),
            global_direction(orientation),
            ColorConvertFloat4ToU32(s.Style.FlowGrid.Colors[FlowGridCol_DiagramLine])
        );
    }

    void line(const ImVec2 &start, const ImVec2 &end) override {
        const auto &color = s.Style.FlowGrid.Colors[FlowGridCol_DiagramLine];
        const auto width = scale(s.Style.FlowGrid.DiagramWireWidth);
        // ImGui adds {0.5, 0.5} to line points.
        draw_list->AddLine(at(start) - ImVec2{0.5f, 0}, at(end) - ImVec2{0.5f, 0}, ColorConvertFloat4ToU32(color), width);
    }

    void text(const ImVec2 &p, const string &text, const TextStyle &style) override {
        const auto &[color, justify, padding_right, padding_bottom, scale_height, font_style] = style;
        const auto &text_pos = p - ImVec2{padding_right, padding_bottom} - (justify == TextStyle::Left ? ImVec2{} : justify == TextStyle::Middle ? text_size(text) / ImVec2{2, 1} : text_size(text));
        draw_list->AddText(at(text_pos), ColorConvertFloat4ToU32(color), text.c_str());
    }

    void dot(const ImVec2 &p, const ImVec4 &fill_color) override {
        const float radius = scale(s.Style.FlowGrid.DiagramOrientationMarkRadius);
        draw_list->AddCircleFilled(at(p), radius, ColorConvertFloat4ToU32(fill_color));
    }

    ImDrawList *draw_list;
};

struct Node;

Node *root_node; // This diagram is drawn every frame if present.
std::stack<Node *> focused_node_stack;
const Node *hovered_node;

static string get_box_type(Box t);

// An abstract block diagram node
struct Node {
    Tree tree;
    const Count in_count, out_count;
    const vector<Node *> children{};
    const Count descendents = 0; // The number of boxes within this node (recursively).
    const bool is_top_level;
    ImVec2 position; // Populated in `place`
    ImVec2 size; // Populated in `place_size`

    Diagram orientation = DiagramForward;

    Node(Tree t, Count in_count, Count out_count, vector<Node *> children = {}, Count directDescendents = 0)
        : tree(t), in_count(in_count), out_count(out_count), children(std::move(children)),
          descendents(directDescendents + ::ranges::accumulate(this->children | views::transform([](Node *child) { return child->descendents; }), 0)),
        // `DiagramFoldComplexity == 0` means no folding
          is_top_level(int(s.Style.FlowGrid.DiagramFoldComplexity) > 0 && descendents >= Count(s.Style.FlowGrid.DiagramFoldComplexity)) {}

    virtual ~Node() = default;

    inline Node *child(Count i) const { return children[i]; }

    Count io_count(IO io) const { return io == IO_In ? in_count : out_count; };
    Count io_count(IO io, const Count child_index) const { return child_index < children.size() ? children[child_index]->io_count(io) : 0; };
    virtual ImVec2 point(IO io, Count channel) const = 0;

    void place(const DeviceType type, const ImVec2 &new_position, Diagram new_orientation) {
        position = new_position;
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
        if (device.type() == DeviceType_ImGui &&
            (!hovered_node || (is_inside(*hovered_node) && IsMouseHoveringRect(device.at(position), device.at(position + size))))) {
            hovered_node = this;
        }
    };
    inline bool is_lr() const { return ::is_lr(orientation); }
    inline float dir_unit() const { return is_lr() ? 1 : -1; }
    inline bool is_forward() const { return orientation == DiagramForward; }
    inline float orientation_unit() const { return is_forward() ? 1 : -1; }
    inline bool is_inside(const Node &node) const {
        return x() > node.x() && right() < node.right() && y() > node.y() && y() < node.bottom();
    }

    inline float x() const { return position.x; }
    inline float y() const { return position.y; }
    inline float w() const { return size.x; }
    inline float h() const { return size.y; }
    inline float right() const { return x() + w(); }
    inline float bottom() const { return y() + h(); }

    inline ImRect rect() const { return {position, position + size}; }
    inline ImVec2 mid() const { return position + size / 2; }

    inline Node *s1() const { return children[0]; }
    inline Node *s2() const { return children[1]; }

    inline static float WireGap() { return s.Style.FlowGrid.DiagramWireGap; }
    inline static ImVec2 Gap() { return s.Style.FlowGrid.DiagramGap; }
    inline static float XGap() { return Gap().x; }
    inline static float YGap() { return Gap().y; }

    // Debug
    void draw_rect(Device &device) const {
        device.rect(rect(), {.fill_color={0.5, 0.5, 0.5, 0.1}, .stroke_color={0, 0, 1, 1}, .stroke_width = 1});
    }
    void draw_type(Device &device) const {
        const string &type = get_box_type(tree); // todo cache this at construction time if we ever use it outside the debug hover context
        const string &type_label = type.empty() ? "Unknown type" : type; // todo instead of unknown type, use inner if present
        const static float padding = 2;
        device.rect({position, position + text_size(type_label) + ImVec2{padding, padding} * 2}, {.fill_color={0.5, 0.5, 0.5, 0.3}});
        device.text(position, type_label, {.color={0, 0, 1, 1}, .justify=TextStyle::Justify::Left, .padding_right=-padding, .padding_bottom=-padding});
    }
    void draw_channel_labels(Device &device) const {
        for (const IO io: IO_All) {
            for (Count channel = 0; channel < io_count(io); channel++) {
                device.text(
                    point(io, channel),
                    format("{}:{}", capitalize(to_string(io, true)), channel),
                    {.color={0, 0, 1, 1}, .justify=TextStyle::Justify::Right, .padding_right=4, .padding_bottom=6, .scale_height=1.3, .font_style=TextStyle::FontStyle::Bold}
                );
                device.circle(point(io, channel), 3, {0, 0, 1, 1}, {0, 0, 0, 1});
            }
        }
    }
    void draw_child_channel_labels(Device &device) const {
        for (const IO io: IO_All) {
            for (Count ci = 0; ci < children.size(); ci++) {
                for (Count channel = 0; channel < io_count(io, ci); channel++) {
                    device.text(
                        child(ci)->point(io, channel),
                        format("C{}->{}:{}", ci, capitalize(to_string(io, true)), channel),
                        {.color={1, 0, 0, 1}, .justify=TextStyle::Justify::Right, .padding_right=4, .scale_height=0.9, .font_style=TextStyle::FontStyle::Bold}
                    );
                    device.circle(child(ci)->point(io, channel), 2, {1, 0, 0, 1}, {0, 0, 0, 1});
                }
            }
        }
    }

protected:
    virtual void _place_size(DeviceType) = 0;
    virtual void _place(DeviceType) {};
    virtual void _draw(Device &) const {}
};

static inline ImVec2 scale(const ImVec2 &p) { return p * get_scale(); }
static inline ImRect scale(const ImRect &r) { return {scale(r.Min), scale(r.Max)}; }
static inline float scale(const float f) { return f * get_scale().y; }
static inline ImVec2 get_scale() {
    if (s.Style.FlowGrid.DiagramScaleFill && !focused_node_stack.empty() && GetCurrentWindowRead()) {
        const auto *focused_node = focused_node_stack.top();
        return GetWindowSize() / focused_node->size;
    }
    return s.Style.FlowGrid.DiagramScale;
}

static const char *getTreeName(Tree t) {
    Tree name;
    return getDefNameProperty(t, name) ? tree2str(name) : nullptr;
}

// Hex address (without the '0x' prefix)
static string unique_id(const void *instance) { return format("{:x}", reinterpret_cast<std::uintptr_t>(instance)); }

// Transform the provided tree and id into a unique, length-limited, alphanumeric file name.
// If the tree is not the (singular) process tree, append its hex address (without the '0x' prefix) to make the file name unique.
static string svg_file_name(Tree t) {
    if (!t) return "";

    const string &tree_name = getTreeName(t);
    if (tree_name == "process") return tree_name + ".svg";

    return (views::take_while(tree_name, [](char c) { return std::isalnum(c); }) | views::take(16) | to<string>) + format("-{}", unique_id(t)) + ".svg";
}

void write_svg(Node *node, const fs::path &path) {
    SVGDevice device(path, svg_file_name(node->tree), node->size);
    device.rect(node->rect(), {.fill_color=s.Style.FlowGrid.Colors[FlowGridCol_DiagramBg]});
    node->draw(device);
}

struct IONode : Node {
    IONode(Tree t, Count in_count, Count out_count, vector<Node *> children = {}, Count directDescendents = 0)
        : Node(t, in_count, out_count, std::move(children), directDescendents) {}

    ImVec2 point(IO io, Count i) const override {
        return {
            x() + ((io == IO_In && is_lr()) || (io == IO_Out && !is_lr()) ? 0 : w()),
            mid().y - WireGap() * (float(io_count(io) - 1) / 2 - float(i)) * orientation_unit()
        };
    }

    ImRect get_frame_rect() const { return {position + Gap(), position + size - Gap()}; }

    // Draw the orientation mark in the corner on the inputs side (respecting global direction setting), like in integrated circuits.
    // Marker on top: Forward orientation. Inputs go from top to bottom.
    // Marker on bottom: Backward orientation. Inputs go from bottom to top.
    void draw_orientation_mark(Device &device) const {
        if (!s.Style.FlowGrid.DiagramOrientationMark) return;

        const auto &rect = get_frame_rect();
        const auto &color = s.Style.FlowGrid.Colors[FlowGridCol_DiagramOrientationMark];
        device.dot(ImVec2{
            is_lr() ? rect.Min.x : rect.Max.x,
            is_forward() ? rect.Min.y : rect.Max.y
        } + ImVec2{dir_unit(), orientation_unit()} * 4, color);
    }
};

// A simple rectangular box with text and inputs/outputs.
struct BlockNode : IONode {
    BlockNode(Tree t, Count in_count, Count out_count, string text, FlowGridCol color = FlowGridCol_DiagramNormal, Node *inner = nullptr)
        : IONode(t, in_count, out_count, {}, 1), text(std::move(text)), color(color), inner(inner) {}

    void _place_size(const DeviceType type) override {
        const float text_w = text_size(text).x;
        size = (Gap() * 2) + ImVec2{
            max(3 * WireGap(), 2 * XGap() + text_w),
            max(3.0f, float(max(in_count, out_count))) * WireGap(),
        };
        if (inner && type == DeviceType_SVG) inner->place_size(type);
    }

    void _place(const DeviceType type) override {
        IONode::_place(type);
        if (inner && type == DeviceType_SVG) inner->place(type);
    }

    void _draw(Device &device) const override {
        const auto &fill_color = s.Style.FlowGrid.Colors[color];
        const auto &text_color = s.Style.FlowGrid.Colors[FlowGridCol_DiagramText];
        const ImRect &rect = get_frame_rect();
        if (device.type() == DeviceType_SVG) {
            auto &svg_device = dynamic_cast<SVGDevice &>(device);
            // todo why is draw called twice for each block with an inner child? (or maybe even every node?)
            //  note this is likely double-writing in ImGui too
            if (inner && !fs::exists(svg_device.directory / svg_file_name(inner->tree))) write_svg(inner, svg_device.directory);
            const string &link = inner ? svg_file_name(tree) : "";
            svg_device.rect(rect, {.fill_color=fill_color, .corner_radius=s.Style.FlowGrid.DiagramBoxCornerRadius}, link);
            svg_device.text(mid(), text, {.color=text_color}, link);
        } else {
            const ImRect &scaled_rect = scale(ImRect{position + Gap(), position + size - Gap()});
            const auto cursor_pos = GetCursorPos();
            SetCursorPos(scaled_rect.Min);
            PushStyleVar(ImGuiStyleVar_FrameRounding, s.Style.FlowGrid.DiagramBoxCornerRadius);
            PushStyleColor(ImGuiCol_Button, fill_color);
            PushStyleColor(ImGuiCol_Text, text_color);
            if (!inner) {
                // Emulate disabled behavior, but without making color dimmer, by just allowing clicks but not changing color.
                PushStyleColor(ImGuiCol_ButtonHovered, fill_color);
                PushStyleColor(ImGuiCol_ButtonActive, fill_color);
            }
            if (Button(format("{}##{}", text, unique_id(this)).c_str(), scaled_rect.GetSize()) && inner) focused_node_stack.push(inner);
            if (!inner) PopStyleColor(2);
            PopStyleColor(2);
            PopStyleVar(1);
            SetCursorPos(cursor_pos);
        }
        draw_orientation_mark(device);
        draw_connections(device);
    }

    void draw_connections(Device &device) const {
        const ImVec2 &arrow_size = s.Style.FlowGrid.DiagramArrowSize;
        const ImVec2 d = {dir_unit() * XGap(), 0};
        const ImVec2 arrow_d = {dir_unit() * arrow_size.x, 0};
        for (const IO io: IO_All) {
            const bool in = io == IO_In;
            for (Count i = 0; i < io_count(io); i++) {
                const auto &p = point(io, i);
                device.line(in ? p : p - d, in ? p + d - arrow_d : p);
                if (in) device.arrow(p + d, orientation); // Input arrows
            }
        }
    }

    const string text;
    const FlowGridCol color;
    Node *inner;
};

// Simple cables (identity box) in parallel.
struct CableNode : Node {
    CableNode(Tree t, Count n = 1) : Node(t, n, n) {}

    // The width of a cable is null, so its input and output connection points are the same.
    void _place_size(const DeviceType) override { size = {0, float(in_count) * WireGap()}; }

    // Place the communication points vertically spaced by `WireGap`.
    void _place(const DeviceType) override {
        for (Count i = 0; i < in_count; i++) {
            const float dx = WireGap() * (float(i) + 0.5f);
            points[i] = position + ImVec2{0, is_lr() ? dx : h() - dx};
        }
    }

    ImVec2 point(IO, Count i) const override { return points[i]; }

private:
    vector<ImVec2> points{in_count};
};

// An inverter is a circle followed by a triangle.
// It corresponds to '*(-1)', and it's used to create more compact diagrams.
struct InverterNode : BlockNode {
    InverterNode(Tree t) : BlockNode(t, 1, 1, "-1", FlowGridCol_DiagramInverter) {}

    void _place_size(const DeviceType) override { size = ImVec2{2.5f, 1} * WireGap(); }

    void _draw(Device &device) const override {
        const float radius = s.Style.FlowGrid.DiagramInverterRadius;
        const ImVec2 p1 = {w() - 2 * XGap(), 1 + (h() - 1) / 2};
        const auto tri_a = position + ImVec2{XGap() + (is_lr() ? 0 : p1.x), 0};
        const auto tri_b = tri_a + ImVec2{dir_unit() * (p1.x - 2 * radius) + (is_lr() ? 0 : x()), p1.y};
        const auto tri_c = tri_a + ImVec2{0, h()};
        device.circle(tri_b + ImVec2{dir_unit() * radius, 0}, radius, {0, 0, 0, 0}, s.Style.FlowGrid.Colors[color]);
        device.triangle(tri_a, tri_b, tri_c, s.Style.FlowGrid.Colors[color]);
        draw_connections(device);
    }
};

// Cable termination
struct CutNode : Node {
    // A Cut is represented by a small black dot.
    // It has 1 input and no output.
    CutNode(Tree t) : Node(t, 1, 0) {}

    // 0 width and 1 height, for the wire.
    void _place_size(const DeviceType) override { size = {0, 1}; }

    // A cut is represented by a small black dot.
    void _draw(Device &) const override {
        // device.circle(point, WireGap() / 8);
    }

    // A Cut has only one input point
    ImVec2 point(IO io, Count) const override {
        assert(io == IO_In);
        return {x(), mid().y};
    }
};

struct ParallelNode : Node {
    ParallelNode(Tree t, Node *s1, Node *s2)
        : Node(t, s1->in_count + s2->in_count, s1->out_count + s2->out_count, {s1, s2}) {}

    void _place_size(const DeviceType) override { size = {max(s1()->w(), s2()->w()), s1()->h() + s2()->h()}; }
    void _place(const DeviceType type) override {
        auto *top = children[is_forward() ? 0 : 1];
        auto *bottom = children[is_forward() ? 1 : 0];
        top->place(type, position + ImVec2{(w() - top->w()) / 2, 0}, orientation);
        bottom->place(type, position + ImVec2{(w() - bottom->w()) / 2, top->h()}, orientation);
    }

    void _draw(Device &device) const override {
        for (const IO io: IO_All) {
            for (Count i = 0; i < io_count(io); i++) {
                device.line(point(io, i), i < io_count(io, 0) ? child(0)->point(io, i) : child(1)->point(io, i - io_count(io, 0)));
            }
        }
    }

    ImVec2 point(IO io, Count i) const override {
        const float dx = (io == IO_In ? -1 : 1) * dir_unit();
        return i < io_count(io, 0) ?
               child(0)->point(io, i) + ImVec2{dx * (w() - s1()->w()) / 2, 0} :
               child(1)->point(io, i - io_count(io, 0)) + ImVec2{dx * (w() - s2()->w()) / 2, 0};
    }
};

// Place and connect two diagrams in recursive composition
struct RecursiveNode : Node {
    RecursiveNode(Tree t, Node *s1, Node *s2) : Node(t, s1->in_count - s2->out_count, s1->out_count, {s1, s2}) {
        assert(s1->in_count >= s2->out_count);
        assert(s1->out_count >= s2->in_count);
    }

    void _place_size(const DeviceType) override {
        size = {
            max(s1()->w(), s2()->w()) + 2 * WireGap() * float(max(io_count(IO_In, 1), io_count(IO_Out, 1))),
            s1()->h() + s2()->h()
        };
    }

    // The two nodes are centered vertically, stacked on top of each other, with stacking order dependent on orientation.
    void _place(const DeviceType type) override {
        auto *top_node = children[is_forward() ? 1 : 0];
        auto *bottom_node = children[is_forward() ? 0 : 1];
        top_node->place(type, position + ImVec2{(w() - top_node->w()) / 2, 0}, DiagramReverse);
        bottom_node->place(type, position + ImVec2{(w() - bottom_node->w()) / 2, top_node->h()}, DiagramForward);
    }

    void _draw(Device &device) const override {
        const float dw = orientation_unit() * WireGap();
        // Out0->In1 feedback connections
        for (Count i = 0; i < io_count(IO_In, 1); i++) {
            const auto &in1 = child(1)->point(IO_In, i);
            const auto &out0 = child(0)->point(IO_Out, i);
            const auto &from = ImVec2{is_lr() ? max(in1.x, out0.x) : min(in1.x, out0.x), out0.y} + ImVec2{float(i) * dw, 0};
            // Draw the delay sign of a feedback connection (three sides of a square centered around the feedback source point).
            const auto &corner1 = from - ImVec2{dw, dw} / ImVec2{4, 2};
            const auto &corner2 = from + ImVec2{dw, -dw} / ImVec2{4, 2};
            device.line(from - ImVec2{dw / 4, 0}, corner1);
            device.line(corner1, corner2);
            device.line(corner2, from + ImVec2{dw / 4, 0});
            // Draw the feedback line
            const ImVec2 &bend = {from.x, in1.y};
            device.line(from - ImVec2{0, dw / 2}, bend);
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
        return {lr ? x() : right(), child(0)->point(io, i + (io == IO_In ? io_count(IO_Out, 1) : 0)).y};
    }
};

struct BinaryNode : Node {
    BinaryNode(Tree t, Node *s1, Node *s2) : Node(t, s1->in_count, s2->out_count, {s1, s2}) {}

    ImVec2 point(IO io, Count i) const override { return child(io == IO_In ? 0 : 1)->point(io, i); }

    void _place_size(const DeviceType) override { size = {s1()->w() + s2()->w() + horizontal_gap(), max(s1()->h(), s2()->h())}; }

    // Place the two components horizontally, centered, with enough space for the connections.
    void _place(const DeviceType type) override {
        auto *left = children[is_lr() ? 0 : 1];
        auto *right = children[is_lr() ? 1 : 0];
        left->place(type, position + ImVec2{0, max(0.0f, right->h() - left->h()) / 2}, orientation);
        right->place(type, position + ImVec2{left->w() + horizontal_gap(), max(0.0f, left->h() - right->h()) / 2}, orientation);
    }

    virtual float horizontal_gap() const { return (s1()->h() + s2()->h()) * s.Style.FlowGrid.DiagramBinaryHorizontalGapRatio; }
};

struct SequentialNode : BinaryNode {
    // The components s1 and s2 must be "compatible" (s1: n->m and s2: m->q).
    SequentialNode(Tree t, Node *s1, Node *s2) : BinaryNode(t, s1, s2) {
        assert(s1->out_count == s2->in_count);
    }

    void _place_size(const DeviceType type) override {
        if (s1()->x() == 0 && s1()->y() == 0 && s2()->x() == 0 && s2()->y() == 0) {
            s1()->place(type, {0, max(0.0f, s2()->h() - s1()->h()) / 2}, DiagramForward);
            s2()->place(type, {0, max(0.0f, s1()->h() - s2()->h()) / 2}, DiagramForward);
        }
        BinaryNode::_place_size(type);
    }

    void _place(const DeviceType type) override {
        BinaryNode::_place(type);
        channels_for_direction = {};
        for (Count i = 0; i < io_count(IO_Out, 0); i++) {
            const auto dy = child(1)->point(IO_In, i).y - child(0)->point(IO_Out, i).y;
            channels_for_direction[dy == 0 ? ImGuiDir_None : dy < 0 ? ImGuiDir_Up : ImGuiDir_Down].emplace_back(i);
        }
    }

    void _draw(Device &device) const override {
        if (!s.Style.FlowGrid.DiagramSequentialConnectionZigzag) {
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
                    const Count x_position = is_forward() ? i : channels.size() - i - 1;
                    const float bend_x = from.x + float(x_position) * dir_unit() * WireGap();
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
        map<ImGuiDir, Count> MaxGroupSize; // Store the size of the largest group for each direction.
        for (Count i = 0; i < io_count(IO_Out, 0); i++) {
            const float yd = child(1)->point(IO_In, i).y - child(0)->point(IO_Out, i).y;
            const auto dir = yd < 0 ? ImGuiDir_Up : yd > 0 ? ImGuiDir_Down : ImGuiDir_None;
            size = dir == prev_dir ? size + 1 : 1;
            prev_dir = dir;
            MaxGroupSize[dir] = max(MaxGroupSize[dir], size);
        }

        return WireGap() * float(max(MaxGroupSize[ImGuiDir_Up], MaxGroupSize[ImGuiDir_Down]));
    }

private:
    map<ImGuiDir, vector<Count>> channels_for_direction;
};

// Place and connect two diagrams in merge composition.
// The outputs of the first node are merged to the inputs of the second.
struct MergeNode : BinaryNode {
    MergeNode(Tree t, Node *s1, Node *s2) : BinaryNode(t, s1, s2) {}

    void _draw(Device &device) const override {
        for (Count i = 0; i < io_count(IO_Out, 0); i++) device.line(child(0)->point(IO_Out, i), child(1)->point(IO_In, i % io_count(IO_In, 1)));
    }
};

// Place and connect two diagrams in split composition.
// The outputs the first node are distributed to the inputs of the second.
struct SplitNode : BinaryNode {
    SplitNode(Tree t, Node *s1, Node *s2) : BinaryNode(t, s1, s2) {}

    void _draw(Device &device) const override {
        for (Count i = 0; i < io_count(IO_In, 1); i++) device.line(child(0)->point(IO_Out, i % io_count(IO_Out, 0)), child(1)->point(IO_In, i));
    }
};

Node *make_sequential(Tree t, Node *s1, Node *s2) {
    const auto o = s1->out_count;
    const auto i = s2->in_count;
    return new SequentialNode(t,
        o < i ? new ParallelNode(t, s1, new CableNode(t, i - o)) : s1,
        o > i ? new ParallelNode(t, s2, new CableNode(t, o - i)) : s2
    );
}

// A `DecorateNode` is a node surrounded by a dashed rectangle with a label on the top left, and arrows added to the outputs.
// If the number of boxes inside is over the `box_complexity` threshold, add additional padding and draw output arrows.
struct DecorateNode : IONode {
    DecorateNode(Tree t, Node *inner, string text)
        : IONode(t, inner->in_count, inner->out_count, {inner}, 0), text(std::move(text)) {}

    void _place_size(const DeviceType) override {
        const float m = margin(s1());
        size = s1()->size + ImVec2{m, m} * 2;
    }

    void _place(const DeviceType type) override {
        const float m = margin();
        s1()->place(type, position + ImVec2{m, m}, orientation);
    }

    void _draw(Device &device) const override {
        const ImVec2 &arrow_size = s.Style.FlowGrid.DiagramArrowSize;
        const float m = 2 * (is_top_level ? s.Style.FlowGrid.DiagramTopLevelMargin : 0) + s.Style.FlowGrid.DiagramDecorateMargin;
        device.grouprect({position + ImVec2{m, m} / 2, position + size - ImVec2{m, m} / 2}, text);
        for (const IO io: IO_All) {
            const bool has_arrow = io == IO_Out && is_top_level;
            for (Count i = 0; i < io_count(io); i++) {
                device.line(child(0)->point(io, i), point(io, i) - ImVec2{has_arrow ? dir_unit() * arrow_size.x : 0, 0});
                if (has_arrow) device.arrow(point(io, i), orientation);
            }
        }
    }

    ImVec2 point(IO io, Count i) const override {
        return child(0)->point(io, i) + ImVec2{dir_unit() * (io == IO_In ? -1 : 1) * s.Style.FlowGrid.DiagramTopLevelMargin, 0};
    }

    inline float margin(const Node *node = nullptr) const {
        return s.Style.FlowGrid.DiagramDecorateMargin + ((node ? node->is_top_level : is_top_level) ? s.Style.FlowGrid.DiagramTopLevelMargin : 0);
    }

private:
    string text;
};

struct RouteNode : IONode {
    RouteNode(Tree t, Count in_count, Count out_count, vector<int> routes)
        : IONode(t, in_count, out_count), routes(std::move(routes)) {}

    void _place_size(const DeviceType) override {
        const float minimal = 3 * WireGap();
        const float h = 2 * YGap() + max(minimal, max(float(in_count), float(out_count)) * WireGap());
        size = {2 * XGap() + max(minimal, h * 0.75f), h};
    }

    void _draw(Device &device) const override {
        if (s.Style.FlowGrid.DiagramRouteFrame) {
            device.rect(get_frame_rect(), {.fill_color={0.93, 0.93, 0.65, 1}}); // todo move to style
            draw_orientation_mark(device);
            // Input arrows
            for (Count i = 0; i < io_count(IO_In); i++) device.arrow(point(IO_In, i) + ImVec2{dir_unit() * XGap(), 0}, orientation);
        }

        // Input/output & route wires
        const auto d = ImVec2{dir_unit() * XGap(), 0};
        for (const IO io: IO_All) {
            const bool in = io == IO_In;
            for (Count i = 0; i < io_count(io); i++) {
                const auto &p = point(io, i);
                device.line(in ? p : p - d, in ? p + d : p);
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
    const vector<int> routes; // Route description: s1,d2,s2,d2,...
};

static bool isBoxBinary(Tree t, Tree &x, Tree &y) {
    return isBoxPar(t, x, y) || isBoxSeq(t, x, y) || isBoxSplit(t, x, y) || isBoxMerge(t, x, y) || isBoxRec(t, x, y);
}

// Generate a 1->0 block node for an input slot.
static Node *make_input_slot(Tree t) { return new BlockNode(t, 1, 0, getTreeName(t), FlowGridCol_DiagramSlot); }

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

static inline string print_tree(Tree tree) {
    const auto &str = printBox(tree, false);
    return str.substr(0, str.size() - 1); // Last character is a newline.
}

// Collect the leaf numbers of tree `t` into vector `v`.
// Return true if `t` is a number or a parallel tree of numbers.
static bool isIntTree(Tree t, vector<int> &v) {
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

// Convert user interface box into a textual representation
static string get_ui_description(Tree box) {
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

// Convert user interface box into a label that can be used to retrieve widget details
//static string get_ui_label(const Tree box) {
//    Tree t1, label, cur, min, max, step, chan;
//    if (isBoxButton(box, label)
//        || isBoxCheckbox(box, label)
//        || isBoxVSlider(box, label, cur, min, max, step)
//        || isBoxHSlider(box, label, cur, min, max, step)
//        || isBoxVGroup(box, label, t1)
//        || isBoxHGroup(box, label, t1)
//        || isBoxTGroup(box, label, t1)
//        || isBoxHBargraph(box, label, min, max)
//        || isBoxVBargraph(box, label, min, max)
//        || isBoxNumEntry(box, label, cur, min, max, step)
//        || isBoxSoundfile(box, label, chan)) {
//        return extractName(label);
//    }
//
//    return "";
//}

static Node *Tree2Node(Tree t, bool allow_links = true);

// Generate the inside node of a block diagram according to its type.
static Node *Tree2NodeNode(Tree t) {
    if (getUserData(t) != nullptr) return new BlockNode(t, xtendedArity(t), 1, xtendedName(t));
    if (isInverter(t)) return new InverterNode(t);

    int i;
    double r;
    if (isBoxInt(t, &i) || isBoxReal(t, &r)) return new BlockNode(t, 0, 1, isBoxInt(t) ? to_string(i) : to_string(r), FlowGridCol_DiagramNumber);
    if (isBoxWaveform(t)) return new BlockNode(t, 0, 2, "waveform{...}");
    if (isBoxWire(t)) return new CableNode(t);
    if (isBoxCut(t)) return new CutNode(t);

    prim0 p0;
    prim1 p1;
    prim2 p2;
    prim3 p3;
    prim4 p4;
    prim5 p5;
    if (isBoxPrim0(t, &p0)) return new BlockNode(t, 0, 1, prim0name(p0));
    if (isBoxPrim1(t, &p1)) return new BlockNode(t, 1, 1, prim1name(p1));
    if (isBoxPrim2(t, &p2)) return new BlockNode(t, 2, 1, prim2name(p2));
    if (isBoxPrim3(t, &p3)) return new BlockNode(t, 3, 1, prim3name(p3));
    if (isBoxPrim4(t, &p4)) return new BlockNode(t, 4, 1, prim4name(p4));
    if (isBoxPrim5(t, &p5)) return new BlockNode(t, 5, 1, prim5name(p5));

    Tree ff;
    if (isBoxFFun(t, ff)) return new BlockNode(t, ffarity(ff), 1, ffname(ff));

    Tree label, chan, type, name, file;
    if (isBoxFConst(t, type, name, file) || isBoxFVar(t, type, name, file)) return new BlockNode(t, 0, 1, tree2str(name));
    if (isBoxButton(t) || isBoxCheckbox(t) || isBoxVSlider(t) || isBoxHSlider(t) || isBoxNumEntry(t)) return new BlockNode(t, 0, 1, get_ui_description(t), FlowGridCol_DiagramUi);
    if (isBoxVBargraph(t) || isBoxHBargraph(t)) return new BlockNode(t, 1, 1, get_ui_description(t), FlowGridCol_DiagramUi);
    if (isBoxSoundfile(t, label, chan)) return new BlockNode(t, 2, 2 + tree2int(chan), get_ui_description(t), FlowGridCol_DiagramUi);

    Tree a, b;
    if (isBoxMetadata(t, a, b)) return Tree2Node(a);

    const bool isVGroup = isBoxVGroup(t, label, a);
    const bool isHGroup = isBoxHGroup(t, label, a);
    const bool isTGroup = isBoxTGroup(t, label, a);
    if (isVGroup || isHGroup || isTGroup) {
        const string groupId = isVGroup ? "v" : isHGroup ? "h" : "t";
        return new DecorateNode(a, Tree2Node(a), groupId + "group(" + extractName(label) + ")");
    }
    if (isBoxSeq(t, a, b)) return make_sequential(t, Tree2Node(a), Tree2Node(b));
    if (isBoxPar(t, a, b)) return new ParallelNode(t, Tree2Node(a), Tree2Node(b));
    if (isBoxSplit(t, a, b)) return new SplitNode(t, Tree2Node(a), Tree2Node(b));
    if (isBoxMerge(t, a, b)) return new MergeNode(t, Tree2Node(a), Tree2Node(b));
    if (isBoxRec(t, a, b)) return new RecursiveNode(t, Tree2Node(a), Tree2Node(b));

    if (isBoxSlot(t, &i)) return new BlockNode(t, 0, 1, getTreeName(t), FlowGridCol_DiagramSlot);

    if (isBoxSymbolic(t, a, b)) {
        // Generate an abstraction node by placing in sequence the input slots and the body.
        auto *input_slots = make_input_slot(a);
        Tree _a, _b;
        while (isBoxSymbolic(b, _a, _b)) {
            input_slots = new ParallelNode(b, input_slots, make_input_slot(_a));
            b = _b;
        }
        auto *abstraction = make_sequential(b, input_slots, Tree2Node(b));
        return getTreeName(t) ? abstraction : new DecorateNode(t, abstraction, "Abstraction");
    }
    if (isBoxEnvironment(t)) return new BlockNode(t, 0, 0, "environment{...}");

    Tree route;
    if (isBoxRoute(t, a, b, route)) {
        int ins, outs;
        vector<int> routes;
        // Build n x m cable routing
        if (isBoxInt(a, &ins) && isBoxInt(b, &outs) && isIntTree(route, routes)) return new RouteNode(t, ins, outs, routes);
        throw std::runtime_error("Invalid route expression : " + print_tree(t));
    }

    throw std::runtime_error("ERROR in Tree2NodeNode, box expression not recognized: " + print_tree(t));
}

string get_box_type(Box t) {
    if (getUserData(t) != nullptr) return format("{}({},{})", xtendedName(t), xtendedArity(t), 1);
    if (isInverter(t)) return "Inverter";
    if (isBoxInt(t)) return "Int";
    if (isBoxReal(t)) return "Real";
    if (isBoxWaveform(t)) return "Waveform";
    if (isBoxWire(t)) return "Cable";
    if (isBoxCut(t)) return "Cut";

    prim0 p0;
    prim1 p1;
    prim2 p2;
    prim3 p3;
    prim4 p4;
    prim5 p5;
    if (isBoxPrim0(t, &p0)) return prim0name(p0);
    if (isBoxPrim1(t, &p1)) return prim1name(p1);
    if (isBoxPrim2(t, &p2)) return prim2name(p2);
    if (isBoxPrim3(t, &p3)) return prim3name(p3);
    if (isBoxPrim4(t, &p4)) return prim4name(p4);
    if (isBoxPrim5(t, &p5)) return prim5name(p5);

    Tree ff;
    if (isBoxFFun(t, ff)) return format("FFun:{}({})", ffname(ff), ffarity(ff));

    Tree label, chan, type, name, file;
    if (isBoxFConst(t, type, name, file)) return format("FConst:{}", tree2str(name));
    if (isBoxFVar(t, type, name, file)) return format("FVar:{}", tree2str(name));
    if (isBoxButton(t)) return "Button";
    if (isBoxCheckbox(t)) return "Checkbox";
    if (isBoxVSlider(t)) return "VSlider";
    if (isBoxHSlider(t)) return "HSlider";
    if (isBoxNumEntry(t)) return "NumEntry";
    if (isBoxVBargraph(t)) return "VBarGraph";
    if (isBoxHBargraph(t)) return "HBarGraph";
    if (isBoxSoundfile(t, label, chan)) return format("Soundfile({},{})", 2, 2 + tree2int(chan));

    Tree a, b;
    if (isBoxVGroup(t)) return "VGroup";
    if (isBoxHGroup(t)) return "HGroup";
    if (isBoxTGroup(t)) return "TGroup";
    if (isBoxSeq(t, a, b)) return "Sequential";
    if (isBoxPar(t, a, b)) return "Parallel";
    if (isBoxSplit(t, a, b)) return "Split";
    if (isBoxMerge(t, a, b)) return "Merge";
    if (isBoxRec(t, a, b)) return "Recursive";

    int i;
    if (isBoxSlot(t, &i)) return format("Slot({})", i);
    if (isBoxEnvironment(t)) return "Environment";

    Tree route;
    if (isBoxRoute(t, a, b, route)) {
        int ins, outs;
        if (isBoxInt(a, &ins) && isBoxInt(b, &outs)) return format("Route({}x{})", ins, outs);
        throw std::runtime_error("Invalid route expression : " + print_tree(t));
    }

    return "";
}

static bool AllowNodeLinks = true; // Set to `false` to draw all nodes inline in one big diagram. Set to `true` to split into files (for SVG rendering).
static map<Tree, bool> IsTreePureRouting{}; // Avoid recomputing pure-routing property. Needs to be reset whenever box changes!

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
static Node *Tree2Node(Tree t, bool allow_links) {
    auto *node = Tree2NodeNode(t);
    if (const char *name = getTreeName(t)) {
        auto *decorate_node = new DecorateNode{t, node, name};
        if (decorate_node->is_top_level && AllowNodeLinks && allow_links) {
            int ins, outs;
            getBoxType(t, &ins, &outs);
            return new BlockNode(t, ins, outs, name, FlowGridCol_DiagramLink, decorate_node);
        }
        if (!isPureRouting(t)) return decorate_node; // Draw a line around the object with its name.
    }

    return node; // normal case
}

void on_box_change(Box box) {
    IsTreePureRouting.clear();
    focused_node_stack = {};
    if (box) {
        root_node = Tree2Node(box, false); // Ensure top-level is not compressed into a link.
        focused_node_stack.push(root_node);
    } else {
        root_node = nullptr;
    }
}

static int prev_fold_complexity = 0; // watch and recompile when it changes

void save_box_svg(const string &path) {
    if (!root_node) return;
    prev_fold_complexity = s.Style.FlowGrid.DiagramFoldComplexity;
    // Render SVG diagram(s)
    fs::remove_all(path);
    fs::create_directory(path);
    auto *node = Tree2Node(root_node->tree, false); // Ensure top-level is not compressed into a link.
    node->place_size(DeviceType_SVG);
    node->place(DeviceType_SVG);
    write_svg(node, path);
}

void Audio::FaustState::FaustDiagram::Draw() const {
    if (!root_node) {
        // todo don't show empty menu bar in this case
        TextUnformatted("Enter a valid Faust program into the 'Faust editor' window to view its diagram."); // todo link to window?
        return;
    }

    if (BeginMenuBar()) {
        if (BeginMenu("File")) {
            fg::MenuItem(action::id<show_save_faust_svg_file_dialog>);
            EndMenu();
        }
        if (BeginMenu("View")) {
            Settings.HoverFlags.DrawMenu();
            EndMenu();
        }
        EndMenuBar();
    }

    if (focused_node_stack.empty()) return;

    if (s.Style.FlowGrid.DiagramFoldComplexity != prev_fold_complexity) {
        prev_fold_complexity = s.Style.FlowGrid.DiagramFoldComplexity;
        on_box_change(root_node->tree);
    }

    {
        // Nav menu
        const bool can_nav = focused_node_stack.size() > 1;
        if (!can_nav) BeginDisabled();
        if (Button("Top")) while (focused_node_stack.size() > 1) focused_node_stack.pop();
        SameLine();
        if (Button("Back")) focused_node_stack.pop();
        if (!can_nav) EndDisabled();
    }

    auto *focused = focused_node_stack.top();
    focused->place_size(DeviceType_ImGui);
    focused->place(DeviceType_ImGui);
    if (!s.Style.FlowGrid.DiagramScaleFill) SetNextWindowContentSize(scale(focused->size));
    BeginChild("Faust diagram inner", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);
    GetCurrentWindow()->FontWindowScale = scale(1);
    GetWindowDrawList()->AddRectFilled(GetWindowPos(), GetWindowPos() + GetWindowSize(),
        ColorConvertFloat4ToU32(s.Style.FlowGrid.Colors[FlowGridCol_DiagramBg]));

    ImGuiDevice device;
    hovered_node = nullptr;
    focused->draw(device);
    if (hovered_node) {
        if (Settings.HoverFlags & FaustDiagramHoverFlags_ShowRect) hovered_node->draw_rect(device);
        if (Settings.HoverFlags & FaustDiagramHoverFlags_ShowType) hovered_node->draw_type(device);
        if (Settings.HoverFlags & FaustDiagramHoverFlags_ShowChannels) hovered_node->draw_channel_labels(device);
        if (Settings.HoverFlags & FaustDiagramHoverFlags_ShowChildChannels) hovered_node->draw_child_channel_labels(device);
    }

    EndChild();
}
