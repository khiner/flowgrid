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

using namespace ImGui;

//-----------------------------------------------------------------------------
// [SECTION] Diagram
//-----------------------------------------------------------------------------

enum DeviceType { DeviceType_ImGui, DeviceType_SVG };
enum DiagramOrientation { DiagramForward, DiagramReverse };

static inline ImVec2 Scale(const ImVec2 &p);
static inline float Scale(float f);

static inline ImGuiDir GlobalDirection(DiagramOrientation orientation) {
    const ImGuiDir dir = s.Style.FlowGrid.Diagram.Direction;
    return (dir == ImGuiDir_Right && orientation == DiagramForward) || (dir == ImGuiDir_Left && orientation == DiagramReverse) ?
           ImGuiDir_Right : ImGuiDir_Left;
}

static inline bool IsLr(DiagramOrientation orientation) { return GlobalDirection(orientation) == ImGuiDir_Right; }

// Device accepts unscaled positions/sizes.
struct Device {
    static constexpr float RectLabelPaddingLeft = 3;

    Device(const ImVec2 &position = {0, 0}) : Position(position) {}
    virtual ~Device() = default;

    virtual DeviceType Type() = 0;

    // All positions received and drawn relative to this device's `Position` and `CursorPosition`.
    // Drawing assumes `SetCursorPos` has been called to set the desired origin.
    virtual void Rect(const ImRect &, const RectStyle &) = 0;
    // Rect with a break in the top-left (to the right of rounding) for a label.
    virtual void LabeledRect(const ImRect &, const string &label, const RectStyle &, const TextStyle &) = 0;

    virtual void Triangle(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImColor &color) = 0;
    virtual void Circle(const ImVec2 &pos, float radius, const ImColor &fill_color, const ImColor &stroke_color) = 0;
    virtual void Arrow(const ImVec2 &pos, DiagramOrientation) = 0;
    virtual void Line(const ImVec2 &start, const ImVec2 &end) = 0;
    virtual void Text(const ImVec2 &pos, const string &text, const TextStyle &) = 0;
    virtual void Dot(const ImVec2 &pos, const ImColor &fill_color) = 0;

    virtual void SetCursorPos(const ImVec2 &scaled_cursor_pos) { CursorPosition = scaled_cursor_pos; }
    void AdvanceCursor(const ImVec2 &unscaled_pos) { SetCursorPos(CursorPosition + Scale(unscaled_pos)); }

    inline ImVec2 At(const ImVec2 &local_pos) const { return Position + CursorPosition + Scale(local_pos); }
    inline ImRect At(const ImRect &local_rect) const { return {At(local_rect.Min), At(local_rect.Max)}; }

    ImVec2 Position{}; // Absolute window position of device
    ImVec2 CursorPosition{}; // In local coordinates, relative to `Position`
};

// ImGui saves font name as "{Name}.{Ext}, {Size}px"
static inline string GetFontName() {
    const string name = GetFont()->GetDebugName();
    return name.substr(0, name.find_first_of('.'));
}
static inline string GetFontPath() {
    const string name = GetFont()->GetDebugName();
    return format("../res/fonts/{}", name.substr(0, name.find_first_of(','))); // Path is relative to build dir.
}
static inline string GetFontBase64() {
    static map<string, string> base64_for_font_name; // avoid recomputing
    const string &font_name = GetFontName();
    if (!base64_for_font_name.contains(font_name)) {
        const string ttf_contents = FileIO::read(GetFontPath());
        string ttf_base64;
        bn::encode_b64(ttf_contents.begin(), ttf_contents.end(), back_inserter(ttf_base64));
        base64_for_font_name[font_name] = ttf_base64;
    }
    return base64_for_font_name.at(font_name);
}

struct SVGDevice : Device {
    SVGDevice(fs::path Directory, string FileName, ImVec2 size) : Directory(std::move(Directory)), FileName(std::move(FileName)) {
        const auto &[w, h] = Scale(size);
        Stream << format(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {} {}")", w, h);
        Stream << (s.Style.FlowGrid.Diagram.ScaleFillHeight ? R"( height="100%">)" : format(R"( width="{}" height="{}">)", w, h));

        // Embed the current font as a base64-encoded string.
        Stream << format(R"(
        <defs><style>
            @font-face{{
                font-family:"{}";
                src:url(data:application/font-woff;charset=utf-8;base64,{}) format("woff");
                font-weight:normal;font-style:normal;
            }}
        </style></defs>)", GetFontName(), GetFontBase64());
    }

    ~SVGDevice() override {
        Stream << "</svg>\n";
        FileIO::write(Directory / FileName, Stream.str());
    }

    DeviceType Type() override { return DeviceType_SVG; }

    static string XmlSanitize(const string &name) {
        static map<char, string> Replacements{{'<', "&lt;"}, {'>', "&gt;"}, {'\'', "&apos;"}, {'"', "&quot;"}, {'&', "&amp;"}};

        auto replaced_name = name;
        for (const auto &[ch, replacement]: Replacements) replaced_name = StringHelper::Replace(replaced_name, ch, replacement);
        return replaced_name;
    }

    // Render an arrow. 'pos' is position of the arrow tip. half_sz.x is length from base to tip. half_sz.y is length on each side.
    static string ArrowPointingAt(const ImVec2 &pos, ImVec2 half_sz, DiagramOrientation orientation, const ImColor &color) {
        const float d = IsLr(orientation) ? -1 : 1;
        return CreateTriangle(ImVec2{pos.x + d * half_sz.x, pos.y - d * half_sz.y}, ImVec2{pos.x + d * half_sz.x, pos.y + d * half_sz.y}, pos, color, color);
    }
    static string CreateTriangle(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImColor &fill_color, const ImColor &stroke_color) {
        return format(R"(<polygon fill="{}" stroke="{}" stroke-width=".5" points="{},{} {},{} {},{}"/>)",
            RgbColor(fill_color), RgbColor(stroke_color), p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
    }
    static string RgbColor(const ImColor &color) {
        const auto &[r, g, b, a] = color.Value * 255;
        return format("rgb({}, {}, {}, {})", r, g, b, a);
    }
    // Scale factor to convert between ImGui font pixel height and SVG `font-size` attr value.
    // Determined empirically to make the two renderings look the same.
    static float GetFontSize() { return Scale(GetTextLineHeight()) * 0.8f; }

    void Rect(const ImRect &local_rect, const RectStyle &style) override {
        const auto &rect = At(local_rect);
        const auto &[fill_color, stroke_color, stroke_width, corner_radius] = style;
        Stream << format(R"(<rect x="{}" y="{}" width="{}" height="{}" rx="{}" style="stroke:{};stroke-width={};fill:{};"/>)",
            rect.Min.x, rect.Min.y, rect.GetWidth(), rect.GetHeight(), corner_radius, RgbColor(stroke_color), stroke_width, RgbColor(fill_color));
    }

    // Only SVG device has a rect-with-link method
    void Rect(const ImRect &local_rect, const RectStyle &style, const string &link) {
        if (!link.empty()) Stream << format(R"(<a href="{}">)", XmlSanitize(link));
        Rect(local_rect, style);
        if (!link.empty()) Stream << "</a>";
    }

    void LabeledRect(const ImRect &local_rect, const string &label, const RectStyle &rect_style, const TextStyle &text_style) override {
        const auto &rect = At(local_rect);
        const auto &tl = rect.Min;
        const auto &tr = rect.GetTR();
        const float label_offset = Scale(max(8.f, rect_style.CornerRadius) + text_style.Padding.Left);
        const float text_x = tl.x + label_offset;
        const ImVec2 &text_right = {min(text_x + TextSize(label).x, tr.x), tr.y};
        const float r = Scale(rect_style.CornerRadius);
        // Going counter-clockwise instead of clockwise, like in the ImGui implementation, since that's what paths expect for corner rounding to work.
        Stream << format(R"(<path d="m{},{} h{} a{},{} 0 00 {},{} v{} a{},{} 0 00 {},{} h{} a{},{} 0 00 {},{} v{} a{},{} 0 00 {},{} h{}" stroke-width="{}" stroke="{}" fill="none"/>)",
            text_x - Scale(text_style.Padding.Left), tl.y, Scale(text_style.Padding.Right - label_offset) + r, r, r, -r, r, // before text to top-left
            rect.GetHeight() - 2 * r, r, r, r, r, // top-left to bottom-left
            rect.GetWidth() - 2 * r, r, r, r, -r, // bottom-left to bottom-right
            -(rect.GetHeight() - 2 * r), r, r, -r, -r, // bottom-right to top-right
            -(tr.x - r - text_right.x), // top-right to after text
            Scale(rect_style.StrokeWidth), RgbColor(rect_style.StrokeColor));
        Stream << format(R"(<text x="{}" y="{}" font-family="{}" font-size="{}" fill="{}" dominant-baseline="middle">{}</text>)",
            text_x, tl.y, GetFontName(), GetFontSize(), RgbColor(text_style.Color), XmlSanitize(label));
    }

    void Triangle(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImColor &color) override {
        Stream << CreateTriangle(At(p1), At(p2), At(p3), {0.f, 0.f, 0.f, 0.f}, color);
    }

    void Circle(const ImVec2 &pos, float radius, const ImColor &fill_color, const ImColor &stroke_color) override {
        const auto [x, y] = At(pos);
        Stream << format(R"(<circle fill="{}" stroke="{}" stroke-width=".5" cx="{}" cy="{}" r="{}"/>)",
            RgbColor(fill_color), RgbColor(stroke_color), x, y, radius);
    }

    void Arrow(const ImVec2 &pos, DiagramOrientation orientation) override {
        Stream << ArrowPointingAt(At(pos), Scale(s.Style.FlowGrid.Diagram.ArrowSize), orientation, s.Style.FlowGrid.Diagram.Colors[FlowGridDiagramCol_Line]);
    }

    void Line(const ImVec2 &start, const ImVec2 &end) override {
        const string line_cap = start.x == end.x || start.y == end.y ? "butt" : "round";
        const auto &start_scaled = At(start);
        const auto &end_scaled = At(end);
        const ImColor &color = s.Style.FlowGrid.Diagram.Colors[FlowGridDiagramCol_Line];
        const auto width = Scale(s.Style.FlowGrid.Diagram.WireWidth);
        Stream << format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}"  style="stroke:{}; stroke-linecap:{}; stroke-width:{};"/>)",
            start_scaled.x, start_scaled.y, end_scaled.x, end_scaled.y, RgbColor(color), line_cap, width);
    }

    void Text(const ImVec2 &pos, const string &text, const TextStyle &style) override {
        const auto &[color, justify, padding, font_style] = style;
        const string anchor = justify.h == HJustify_Left ? "start" : justify.h == HJustify_Middle ? "middle" : "end";
        const string font_style_formatted = font_style == TextStyle::FontStyle::Italic ? "italic" : "normal";
        const string font_weight = font_style == TextStyle::FontStyle::Bold ? "bold" : "normal";
        const auto &p = At(pos - ImVec2{style.Padding.Right, style.Padding.Bottom});
        Stream << format(R"(<text x="{}" y="{}" font-family="{}" font-style="{}" font-weight="{}" font-size="{}" text-anchor="{}" fill="{}" dominant-baseline="middle">{}</text>)",
            p.x, p.y, GetFontName(), font_style_formatted, font_weight, GetFontSize(), anchor, RgbColor(color), XmlSanitize(text));
    }

    // Only SVG device has a text-with-link method
    void Text(const ImVec2 &pos, const string &str, const TextStyle &style, const string &link) {
        if (!link.empty()) Stream << format(R"(<a href="{}">)", XmlSanitize(link));
        Text(pos, str, style);
        if (!link.empty()) Stream << "</a>";
    }

    void Dot(const ImVec2 &pos, const ImColor &fill_color) override {
        const auto &p = At(pos);
        const float radius = Scale(s.Style.FlowGrid.Diagram.OrientationMarkRadius);
        Stream << format(R"(<circle cx="{}" cy="{}" r="{}" fill="{}"/>)", p.x, p.y, radius, RgbColor(fill_color));
    }

    fs::path Directory;
    string FileName;

private:
    std::stringstream Stream;
};

struct ImGuiDevice : Device {
    ImGuiDevice() : Device(GetCursorScreenPos()), DrawList(GetWindowDrawList()) {}

    DeviceType Type() override { return DeviceType_ImGui; }

    void SetCursorPos(const ImVec2 &scaled_cursor_pos) override {
        Device::SetCursorPos(scaled_cursor_pos);
        ImGui::SetCursorScreenPos(At({0, 0}));
    }
    void Rect(const ImRect &local_rect, const RectStyle &style) override {
        const auto &rect = At(local_rect);
        const auto &[fill_color, stroke_color, stroke_width, corner_radius] = style;
        if (fill_color.Value.w != 0) DrawList->AddRectFilled(rect.Min, rect.Max, fill_color, corner_radius);
        if (stroke_color.Value.w != 0) DrawList->AddRect(rect.Min, rect.Max, stroke_color, corner_radius);
    }

    void LabeledRect(const ImRect &local_rect, const string &label, const RectStyle &rect_style, const TextStyle &text_style) override {
        const auto &rect = At(local_rect);
        const auto &padding = text_style.Padding;
        const auto &padding_left = Scale(padding.Left), &padding_right = Scale(padding.Right);
        const float r = Scale(rect_style.CornerRadius);
        const float label_offset_x = max(Scale(8), r) + padding_left;
        const auto &ellipsified_label = Ellipsify(label, rect.GetWidth() - r - label_offset_x - padding_right);

        // Clockwise, starting to right of text
        // todo port this to svg, and use the arg to make rounded rect path go clockwise (there is one).
        const auto &a = rect.Min + ImVec2{0, GetFontSize() / 2}, &b = rect.Max;
        const auto &text_top_left = rect.Min + ImVec2{label_offset_x, 0};
        const auto &rect_start = a + ImVec2{label_offset_x, 0} + ImVec2{TextSize(ellipsified_label).x + padding_left, 0};
        const auto &rect_end = text_top_left + ImVec2{-padding_left, GetFontSize() / 2};
        if (r < 1.5f) {
            DrawList->PathLineTo(rect_start);
            DrawList->PathLineTo({b.x, a.y});
            DrawList->PathLineTo(b);
            DrawList->PathLineTo({a.x, b.y});
            DrawList->PathLineTo(a);
            DrawList->PathLineTo(rect_end);
        } else {
            if (rect_start.x < b.x - r) DrawList->PathLineTo(rect_start);
            DrawList->PathArcToFast({b.x - r, a.y + r}, r, 9, 12);
            DrawList->PathArcToFast({b.x - r, b.y - r}, r, 0, 3);
            DrawList->PathArcToFast({a.x + r, b.y - r}, r, 3, 6);
            DrawList->PathArcToFast({a.x + r, a.y + r}, r, 6, 9);
            if (rect_end.x > a.x + r) DrawList->PathLineTo(rect_end);
        }

        DrawList->PathStroke(rect_style.StrokeColor, ImDrawFlags_None, Scale(rect_style.StrokeWidth));
        DrawList->AddText(text_top_left, text_style.Color, ellipsified_label.c_str());
    }

    void Triangle(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImColor &color) override {
        DrawList->AddTriangle(At(p1), At(p2), At(p3), color);
    }

    void Circle(const ImVec2 &p, float radius, const ImColor &fill_color, const ImColor &stroke_color) override {
        if (fill_color.Value.w != 0) DrawList->AddCircleFilled(At(p), Scale(radius), fill_color);
        if (stroke_color.Value.w != 0) DrawList->AddCircle(At(p), Scale(radius), stroke_color);
    }

    void Arrow(const ImVec2 &p, DiagramOrientation orientation) override {
        RenderArrowPointingAt(DrawList,
            At(p) + ImVec2{0, 0.5f},
            Scale(s.Style.FlowGrid.Diagram.ArrowSize),
            GlobalDirection(orientation),
            s.Style.FlowGrid.Diagram.Colors[FlowGridDiagramCol_Line]
        );
    }

    void Line(const ImVec2 &start, const ImVec2 &end) override {
        const U32 color = s.Style.FlowGrid.Diagram.Colors[FlowGridDiagramCol_Line];
        const float width = Scale(s.Style.FlowGrid.Diagram.WireWidth);
        // ImGui adds {0.5, 0.5} to line points.
        DrawList->AddLine(At(start) - ImVec2{0.5f, 0}, At(end) - ImVec2{0.5f, 0}, color, width);
    }

    void Text(const ImVec2 &p, const string &text, const TextStyle &style) override {
        const auto &[color, justify, padding, font_style] = style;
        const auto &size = TextSize(text);
        DrawList->AddText(
            At(p - ImVec2{padding.Right, padding.Bottom}) -
                ImVec2{justify.h == HJustify_Left ? 0 : justify.h == HJustify_Middle ? size.x / 2 : size.x,
                       justify.v == VJustify_Top ? 0 : justify.v == VJustify_Middle ? size.y / 2 : size.y,},
            color, text.c_str());
    }

    void Dot(const ImVec2 &p, const ImColor &fill_color) override {
        const float radius = Scale(s.Style.FlowGrid.Diagram.OrientationMarkRadius);
        DrawList->AddCircleFilled(At(p), radius, fill_color);
    }

    ImDrawList *DrawList;
};

static string GetTreeName(Tree tree) {
    Tree name;
    return getDefNameProperty(tree, name) ? tree2str(name) : "";
}

struct Node;

Node *RootNode; // This diagram is drawn every frame if present.
std::stack<Node *> FocusedNodeStack;
const Node *HoveredNode;

static string GetBoxType(Box t);

static map<const Node *, Count> DrawCountForNode{};

// Hex address (without the '0x' prefix)
static string UniqueId(const void *instance) { return format("{:x}", reinterpret_cast<std::uintptr_t>(instance)); }

// An abstract block diagram node.
// todo next up:
//  - Fix saving to SVG with `DecorateRootNode = false` (and generally get it back to its former self).
struct Node {
    inline static float WireGap() { return s.Style.FlowGrid.Diagram.WireGap; }

    Tree FaustTree;
    const vector<Node *> Children{};
    const Count InCount, OutCount;
    const Count Descendents = 0; // The number of boxes within this node (recursively).
    const string Text;

    ImVec2 Position; // Relative to parent. Set in `Place`.
    ImVec2 Size; // Set in `PlaceSize`.
    DiagramOrientation Orientation = DiagramForward; // Set in `Place`.

    Node(Tree tree, Count in_count, Count out_count, string text = "", vector<Node *> children = {}, Count direct_descendents = 0)
        : FaustTree(tree), Children(std::move(children)), InCount(in_count), OutCount(out_count),
          Descendents(direct_descendents + ::ranges::accumulate(this->Children | views::transform([](Node *child) { return child->Descendents; }), 0)),
          Text(!text.empty() ? std::move(text) : GetTreeName(tree)) {}

    virtual ~Node() = default;

    inline Node *Child(Count i) const { return Children[i]; }

    Count IoCount(IO io) const { return io == IO_In ? InCount : OutCount; };
    Count IoCount(IO io, const Count child_index) const { return child_index < Children.size() ? Children[child_index]->IoCount(io) : 0; };
    ImVec2 Point(Count child, IO io, Count channel) const { return Child(child)->Position + Child(child)->Point(io, channel); }

    void Place(const DeviceType type, const ImVec2 &position, DiagramOrientation orientation) {
        Position = position;
        Orientation = orientation;
        DoPlace(type);
    }
    void PlaceSize(const DeviceType type) {
        for (auto *child: Children) child->PlaceSize(type);
        DoPlaceSize(type);
    }
    void Place(const DeviceType type) { DoPlace(type); }
    void Draw(Device &device) const {
        DrawCountForNode[this] += 1;
        // todo only log in release build
        if (DrawCountForNode[this] > 1) throw std::runtime_error(format("Node drawn more than once in a single frame. Draw count: {}", DrawCountForNode[this]));

        const auto before_cursor = device.CursorPosition;
        device.AdvanceCursor(Position);

        const bool is_imgui = device.Type() == DeviceType_ImGui;
        if (is_imgui) {
            PushID(UniqueId(FaustTree).c_str());
//            InvisibleButton("", Scale(Size));
//            SetItemAllowOverlap();
            if (IsMouseHoveringRect(device.At({0, 0}), device.At(Size))) HoveredNode = this;
        }

        DoDraw(device);
        DrawConnections(device);
        for (auto *child: Children) child->Draw(device);

        if (is_imgui) PopID();

        device.SetCursorPos(before_cursor);
    };

    virtual ImVec2 Margin() const { return s.Style.FlowGrid.Diagram.NodeMargin; }
    virtual ImVec2 Padding() const { return s.Style.FlowGrid.Diagram.NodePadding; } // Currently only actually used for `BlockNode` text
    inline float XMargin() const { return Margin().x; }
    inline float YMargin() const { return Margin().y; }

    inline float W() const { return Size.x; }
    inline float H() const { return Size.y; }
    inline operator ImRect() const { return {{0, 0}, Size}; }

    inline Node *C1() const { return Children[0]; }
    inline Node *C2() const { return Children[1]; }

    inline bool IsForward() const { return Orientation == DiagramForward; }
    inline float OrientationUnit() const { return IsForward() ? 1 : -1; }

    inline bool IsLr() const { return ::IsLr(Orientation); }
    inline float DirUnit() const { return IsLr() ? 1 : -1; }
    inline float DirUnit(IO io) const { return DirUnit() * (io == IO_In ? 1.f : -1.f); }

    virtual void DrawConnections(Device &) const {}

    // Debug
    void DrawRect(Device &device) const {
        device.Rect(*this, {.FillColor={0.5f, 0.5f, 0.5f, 0.1f}, .StrokeColor={0.f, 0.f, 1.f, 1.f}, .StrokeWidth=1});
    }
    void DrawType(Device &device) const {
        const string &type = GetBoxType(FaustTree); // todo cache this at construction time if we ever use it outside the debug hover context
        const string &type_label = type.empty() ? "Unknown type" : type; // todo instead of unknown type, use inner if present
        const static float padding = 2;
        device.Rect({{0, 0}, TextSize(type_label) + padding * 2}, {.FillColor={0.5f, 0.5f, 0.5f, 0.3f}});
        device.Text({0, 0}, type_label, {.Color={0.f, 0.f, 1.f, 1.f}, .Justify={HJustify_Left, VJustify_Bottom}});
    }
    void DrawChannelLabels(Device &device) const {
        for (const IO io: IO_All) {
            for (Count channel = 0; channel < IoCount(io); channel++) {
                device.Text(
                    Point(io, channel),
                    format("{}:{}", Capitalize(to_string(io, true)), channel),
                    {.Color={0.f, 0.f, 1.f, 1.f}, .Justify={HJustify_Right, VJustify_Middle}, .Padding={6, 4}, .FontStyle=TextStyle::FontStyle::Bold}
                );
                device.Circle(Point(io, channel), 3, {0.f, 0.f, 1.f, 1.f}, {0.f, 0.f, 0.f, 1.f});
            }
        }
    }
    void DrawChildChannelLabels(Device &device) const {
        for (const IO io: IO_All) {
            for (Count ci = 0; ci < Children.size(); ci++) {
                for (Count channel = 0; channel < IoCount(io, ci); channel++) {
                    device.Text(
                        Point(ci, io, channel),
                        format("C{}->{}:{}", ci, Capitalize(to_string(io, true)), channel),
                        {.Color={1.f, 0.f, 0.f, 1.f}, .Justify={HJustify_Right, VJustify_Middle}, .Padding={0, 4, 0, 0}, .FontStyle=TextStyle::FontStyle::Bold}
                    );
                    device.Circle(Point(ci, io, channel), 2, {1.f, 0.f, 0.f, 1.f}, {0.f, 0.f, 0.f, 1.f});
                }
            }
        }
    }

    void MarkFrame() {
        DrawCountForNode[this] = 0;
        for (auto *child: Children) child->MarkFrame();
    }

    virtual ImVec2 Point(IO io, Count channel) const {
        return {
            ((io == IO_In && IsLr()) || (io == IO_Out && !IsLr()) ? 0 : W()),
            Size.y / 2 - WireGap() * (float(IoCount(io) - 1) / 2 - float(channel)) * OrientationUnit()
        };
    }
protected:

    virtual void DoPlaceSize(DeviceType) = 0;
    virtual void DoPlace(DeviceType) = 0;
    virtual void DoDraw(Device &) const {}

    ImRect GetFrameRect() const { return {Margin(), Size - Margin()}; }

    // Draw the orientation mark in the corner on the inputs side (respecting global direction setting), like in integrated circuits.
    // Marker on top: Forward orientation. Inputs go from top to bottom.
    // Marker on bottom: Backward orientation. Inputs go from bottom to top.
    void DrawOrientationMark(Device &device) const {
        if (!s.Style.FlowGrid.Diagram.OrientationMark) return;

        const auto &rect = GetFrameRect();
        const U32 color = s.Style.FlowGrid.Diagram.Colors[FlowGridDiagramCol_OrientationMark];
        device.Dot(ImVec2{
            IsLr() ? rect.Min.x : rect.Max.x,
            IsForward() ? rect.Min.y : rect.Max.y
        } + ImVec2{DirUnit(), OrientationUnit()} * 4, color);
    }
};

static inline float GetScale() {
    if (!s.Style.FlowGrid.Diagram.ScaleFillHeight || FocusedNodeStack.empty() || !GetCurrentWindowRead()) return s.Style.FlowGrid.Diagram.Scale;
    return GetWindowHeight() / FocusedNodeStack.top()->H();
}

static inline ImVec2 Scale(const ImVec2 &p) { return p * GetScale(); }
static inline float Scale(const float f) { return f * GetScale(); }

// Transform the provided tree and id into a unique, length-limited, alphanumeric file name.
// If the tree is not the (singular) process tree, append its hex address (without the '0x' prefix) to make the file name unique.
static string SvgFileName(Tree tree) {
    if (!tree) return "";

    const string &tree_name = GetTreeName(tree);
    if (tree_name == "process") return tree_name + ".svg";

    return (views::take_while(tree_name, [](char c) { return std::isalnum(c); }) | views::take(16) | to<string>) + format("-{}", UniqueId(tree)) + ".svg";
}

void WriteSvg(Node *node, const fs::path &path) {
    SVGDevice device(path, SvgFileName(node->FaustTree), node->Size);
    device.Rect(*node, {.FillColor=s.Style.FlowGrid.Diagram.Colors[FlowGridDiagramCol_Bg]}); // todo this should be done in both cases
    node->Draw(device);
}

// A simple rectangular box with text and inputs/outputs.
struct BlockNode : Node {
    BlockNode(Tree tree, Count in_count, Count out_count, string text, FlowGridCol color = FlowGridDiagramCol_Normal, Node *inner = nullptr)
        : Node(tree, in_count, out_count, std::move(text), {}, 1), Color(color), Inner(inner) {}

    void DoPlaceSize(const DeviceType type) override {
        Size = Margin() * 2 + ImVec2{
            max(3.f * WireGap(), TextSize(Text).x + Padding().x * 2),
            max(3.f * WireGap(), float(max(InCount, OutCount)) * WireGap()),
        };
        if (Inner && type == DeviceType_SVG) Inner->PlaceSize(type);
    }

    void DoPlace(const DeviceType type) override { if (Inner && type == DeviceType_SVG) Inner->Place(type); }

    void DoDraw(Device &device) const override {
        U32 fill_color = s.Style.FlowGrid.Diagram.Colors[Color];
        const U32 text_color = s.Style.FlowGrid.Diagram.Colors[FlowGridDiagramCol_Text];
        const auto &local_rect = GetFrameRect();
        const auto &size = local_rect.GetSize();

        if (device.Type() == DeviceType_SVG) {
            auto &svg_device = dynamic_cast<SVGDevice &>(device);
            if (Inner && !fs::exists(svg_device.Directory / SvgFileName(Inner->FaustTree))) WriteSvg(Inner, svg_device.Directory);
            const string &link = Inner ? SvgFileName(FaustTree) : "";
            svg_device.Rect(local_rect, {.FillColor=fill_color, .CornerRadius=s.Style.FlowGrid.Diagram.BoxCornerRadius}, link);
            svg_device.Text(Size / 2, Text, {.Color=text_color}, link);
        } else {
            const auto before_cursor = device.CursorPosition;
            device.AdvanceCursor(local_rect.Min); // todo this pattern should be RIAA style

            if (Inner) {
                bool hovered, held;
                if (fg::InvisibleButton(Scale(size), &hovered, &held)) FocusedNodeStack.push(Inner);
                fill_color = GetColorU32(held ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
            }
            RenderFrame(device.At({0, 0}), device.At(size), fill_color, false, s.Style.FlowGrid.Diagram.BoxCornerRadius);
            device.Text(size / 2, Text, {.Color=text_color});

            device.SetCursorPos(before_cursor);
        }
        DrawOrientationMark(device);
    }

    void DrawConnections(Device &device) const override {
        for (const IO io: IO_All) {
            const bool in = io == IO_In;
            const float arrow_width = in ? s.Style.FlowGrid.Diagram.ArrowSize.X : 0.f;
            for (Count channel = 0; channel < IoCount(io); channel++) {
                const auto &channel_point = Point(io, channel);
                const auto &b = channel_point + ImVec2{(XMargin() - arrow_width) * DirUnit(io), 0};
                device.Line(channel_point, b);
                if (in) device.Arrow(b + ImVec2{arrow_width, 0}, Orientation);
            }
        }
    }

    const FlowGridCol Color;
    Node *Inner;
};

// Simple cables (identity box) in parallel.
struct CableNode : Node {
    CableNode(Tree tree, Count n = 1) : Node(tree, n, n) {}

    // The width of a cable is null, so its input and output connection points are the same.
    void DoPlaceSize(const DeviceType) override { Size = {0, float(InCount) * WireGap()}; }

    // Place the communication points vertically spaced by `WireGap`.
    void DoPlace(const DeviceType) override {
        for (Count i = 0; i < InCount; i++) {
            const float dx = WireGap() * (float(i) + 0.5f);
            Points[i] = {0, IsLr() ? dx : H() - dx};
        }
    }

    ImVec2 Point(IO, Count i) const override { return Points[i]; }

private:
    vector<ImVec2> Points{InCount};
};

// An inverter is a circle followed by a triangle.
// It corresponds to '*(-1)', and it's used to create more compact diagrams.
struct InverterNode : BlockNode {
    InverterNode(Tree tree) : BlockNode(tree, 1, 1, "-1", FlowGridDiagramCol_Inverter) {}

    void DoPlaceSize(const DeviceType) override { Size = ImVec2{2.5f, 1} * WireGap(); }

    void DoDraw(Device &device) const override {
        const float radius = s.Style.FlowGrid.Diagram.InverterRadius;
        const ImVec2 p1 = {W() - 2 * XMargin(), 1 + (H() - 1) / 2};
        const auto tri_a = ImVec2{XMargin() + (IsLr() ? 0 : p1.x), 0};
        const auto tri_b = tri_a + ImVec2{DirUnit() * (p1.x - 2 * radius) + (IsLr() ? 0 : W()), p1.y};
        const auto tri_c = tri_a + ImVec2{0, H()};
        device.Circle(tri_b + ImVec2{DirUnit() * radius, 0}, radius, {0.f, 0.f, 0.f, 0.f}, s.Style.FlowGrid.Diagram.Colors[Color]);
        device.Triangle(tri_a, tri_b, tri_c, s.Style.FlowGrid.Diagram.Colors[Color]);
    }
};

// Cable termination
struct CutNode : Node {
    // A Cut is represented by a small black dot.
    // It has 1 input and no output.
    CutNode(Tree tree) : Node(tree, 1, 0) {}

    // 0 width and 1 height, for the wire.
    void DoPlaceSize(const DeviceType) override { Size = {0, 1}; }
    void DoPlace(const DeviceType) override {}

    // A cut is represented by a small black dot.
    void DoDraw(Device &) const override {
        // device.Circle(point, WireGap() / 8);
    }

    // A Cut has only one input point
    ImVec2 Point(IO io, Count) const override {
        assert(io == IO_In);
        return {0, (Size / 2).y};
    }
};

struct ParallelNode : Node {
    ParallelNode(Tree tree, Node *c1, Node *c2)
        : Node(tree, c1->InCount + c2->InCount, c1->OutCount + c2->OutCount, "", {c1, c2}) {}

    void DoPlaceSize(const DeviceType) override { Size = {max(C1()->W(), C2()->W()), C1()->H() + C2()->H()}; }
    void DoPlace(const DeviceType type) override {
        auto *top = Children[IsForward() ? 0 : 1];
        auto *bottom = Children[IsForward() ? 1 : 0];
        top->Place(type, ImVec2{(W() - top->W()) / 2, 0}, Orientation);
        bottom->Place(type, ImVec2{(W() - bottom->W()) / 2, top->H()}, Orientation);
    }

    void DrawConnections(Device &device) const override {
        for (const IO io: IO_All) {
            for (Count i = 0; i < IoCount(io); i++) {
                device.Line(Point(io, i), i < C1()->IoCount(io) ? Node::Point(0, io, i) : Node::Point(1, io, i - C1()->IoCount(io)));
            }
        }
    }

    ImVec2 Point(IO io, Count i) const override {
        const float dx = (io == IO_In ? -1.f : 1.f) * DirUnit();
        return i < C1()->IoCount(io) ?
               Node::Point(0, io, i) + ImVec2{dx * (W() - C1()->W()) / 2, 0} :
               Node::Point(1, io, i - C1()->IoCount(io)) + ImVec2{dx * (W() - C2()->W()) / 2, 0};
    }
};

// Place and connect two diagrams in recursive composition
struct RecursiveNode : Node {
    RecursiveNode(Tree tree, Node *c1, Node *c2) : Node(tree, c1->InCount - c2->OutCount, c1->OutCount, "", {c1, c2}) {
        assert(c1->InCount >= c2->OutCount);
        assert(c1->OutCount >= c2->InCount);
    }

    void DoPlaceSize(const DeviceType) override {
        Size = {
            max(C1()->W(), C2()->W()) + 2 * WireGap() * float(max(IoCount(IO_In, 1), IoCount(IO_Out, 1))),
            C1()->H() + C2()->H()
        };
    }

    // The two nodes are centered vertically, stacked on top of each other, with stacking order dependent on orientation.
    void DoPlace(const DeviceType type) override {
        auto *top_node = Children[IsForward() ? 1 : 0];
        auto *bottom_node = Children[IsForward() ? 0 : 1];
        top_node->Place(type, {(W() - top_node->W()) / 2, 0}, DiagramReverse);
        bottom_node->Place(type, {(W() - bottom_node->W()) / 2, top_node->H()}, DiagramForward);
    }

    void DoDraw(Device &device) const override {
        const float dw = OrientationUnit() * WireGap();
        // Out0->In1 feedback connections
        for (Count i = 0; i < IoCount(IO_In, 1); i++) {
            const auto &in1 = Node::Point(1, IO_In, i);
            const auto &out0 = Node::Point(0, IO_Out, i);
            const auto &from = ImVec2{IsLr() ? max(in1.x, out0.x) : min(in1.x, out0.x), out0.y} + ImVec2{float(i) * dw, 0};
            // Draw the delay sign of a feedback connection (three sides of a square centered around the feedback source point).
            const auto &corner1 = from - ImVec2{dw, dw} / ImVec2{4, 2};
            const auto &corner2 = from + ImVec2{dw, -dw} / ImVec2{4, 2};
            device.Line(from - ImVec2{dw / 4, 0}, corner1);
            device.Line(corner1, corner2);
            device.Line(corner2, from + ImVec2{dw / 4, 0});
            // Draw the feedback line
            const ImVec2 &bend = {from.x, in1.y};
            device.Line(from - ImVec2{0, dw / 2}, bend);
            device.Line(bend, in1);
        }
        // Non-recursive output lines
        for (Count i = 0; i < OutCount; i++) device.Line(Node::Point(0, IO_Out, i), Point(IO_Out, i));
        // Input lines
        for (Count i = 0; i < InCount; i++) device.Line(Point(IO_In, i), Node::Point(0, IO_In, i + C2()->OutCount));
        // Out1->In0 feedfront connections
        for (Count i = 0; i < IoCount(IO_Out, 1); i++) {
            const auto &from = Node::Point(1, IO_Out, i);
            const auto &from_dx = from - ImVec2{dw * float(i), 0};
            const auto &to = Node::Point(0, IO_In, i);
            const ImVec2 &corner1 = {to.x, from_dx.y};
            const ImVec2 &corner2 = {from_dx.x, to.y};
            const ImVec2 &bend = IsLr() ? (from_dx.x > to.x ? corner1 : corner2) : (from_dx.x > to.x ? corner2 : corner1);
            device.Line(from, from_dx);
            device.Line(from_dx, bend);
            device.Line(bend, to);
        }
    }

    ImVec2 Point(IO io, Count i) const override {
        const bool lr = (io == IO_In && IsLr()) || (io == IO_Out && !IsLr());
        return {lr ? 0 : W(), Node::Point(0, io, i + (io == IO_In ? IoCount(IO_Out, 1) : 0)).y};
    }
};

// Split left/right
// todo configurable `Orientation` to switch between LR/TB orientation.
struct BinaryNode : Node {
    BinaryNode(Tree tree, Node *c1, Node *c2) : Node(tree, c1->InCount, c2->OutCount, "", {c1, c2}) {}

    ImVec2 Point(IO io, Count i) const override { return Node::Point(io == IO_In ? 0 : 1, io, i); }

    void DoPlaceSize(const DeviceType) override { Size = {C1()->W() + C2()->W() + HorizontalGap(), max(C1()->H(), C2()->H())}; }

    // Place the two components horizontally, centered, with enough space for the connections.
    void DoPlace(const DeviceType type) override {
        auto *left = Children[IsLr() ? 0 : 1];
        auto *right = Children[IsLr() ? 1 : 0];
        left->Place(type, {0, max(0.f, right->H() - left->H()) / 2}, Orientation);
        right->Place(type, {left->W() + HorizontalGap(), max(0.f, left->H() - right->H()) / 2}, Orientation);
    }

    virtual float HorizontalGap() const { return (C1()->H() + C2()->H()) * s.Style.FlowGrid.Diagram.BinaryHorizontalGapRatio; }
};

// Arrange children left to right
// todo configurable `Orientation` to switch between LR/TB orientation.
struct SequentialNode : BinaryNode {
    // The components c1 and c2 must be "compatible" (c1: n->m and c2: m->q).
    SequentialNode(Tree tree, Node *c1, Node *c2) : BinaryNode(tree, c1, c2) {
        assert(c1->OutCount == c2->InCount);
    }

    void DoPlaceSize(const DeviceType type) override {
        if (C1()->Position.x == 0 && C1()->Position.y == 0 && C2()->Position.x == 0 && C2()->Position.y == 0) {
            C1()->Place(type, {0, max(0.f, C2()->H() - C1()->H()) / 2}, DiagramForward);
            C2()->Place(type, {0, max(0.f, C1()->H() - C2()->H()) / 2}, DiagramForward);
        }
        BinaryNode::DoPlaceSize(type);
    }

    void DoPlace(const DeviceType type) override {
        BinaryNode::DoPlace(type);
        ChannelsForDirection = {};
        for (Count i = 0; i < IoCount(IO_Out, 0); i++) {
            const auto dy = Node::Point(1, IO_In, i).y - Node::Point(0, IO_Out, i).y;
            ChannelsForDirection[dy == 0 ? ImGuiDir_None : dy < 0 ? ImGuiDir_Up : ImGuiDir_Down].emplace_back(i);
        }
    }

    void DrawConnections(Device &device) const override {
        if (!s.Style.FlowGrid.Diagram.SequentialConnectionZigzag) {
            // Draw a straight, potentially diagonal cable.
            for (Count i = 0; i < IoCount(IO_Out, 0); i++) device.Line(Node::Point(0, IO_Out, i), Node::Point(1, IO_In, i));
            return;
        }
        // Draw upward zigzag cables, with the x turning point determined by the index of the connection in the group.
        for (const auto dir: views::keys(ChannelsForDirection)) {
            const auto &channels = ChannelsForDirection.at(dir);
            for (Count i = 0; i < channels.size(); i++) {
                const auto channel = channels[i];
                const auto from = Node::Point(0, IO_Out, channel);
                const auto to = Node::Point(1, IO_In, channel);
                if (dir == ImGuiDir_None) {
                    device.Line(from, to); // Draw a  straight cable
                } else {
                    const Count x_position = IsForward() ? i : channels.size() - i - 1;
                    const float bend_x = from.x + float(x_position) * DirUnit() * WireGap();
                    device.Line(from, {bend_x, from.y});
                    device.Line({bend_x, from.y}, {bend_x, to.y});
                    device.Line({bend_x, to.y}, to);
                }
            }
        }
    }

    // Compute the horizontal gap needed to draw the internal wires.
    // It depends on the largest group of connections that go in the same up/down direction.
    float HorizontalGap() const override {
        if (IoCount(IO_Out, 0) == 0) return 0;

        ImGuiDir prev_dir = ImGuiDir_None;
        Count size = 0;
        map<ImGuiDir, Count> max_group_size; // Store the size of the largest group for each direction.
        for (Count i = 0; i < IoCount(IO_Out, 0); i++) {
            const float yd = Node::Point(1, IO_In, i).y - Node::Point(0, IO_Out, i).y;
            const auto dir = yd < 0 ? ImGuiDir_Up : yd > 0 ? ImGuiDir_Down : ImGuiDir_None;
            size = dir == prev_dir ? size + 1 : 1;
            prev_dir = dir;
            max_group_size[dir] = max(max_group_size[dir], size);
        }

        return WireGap() * float(max(max_group_size[ImGuiDir_Up], max_group_size[ImGuiDir_Down]));
    }

private:
    map<ImGuiDir, vector<Count>> ChannelsForDirection;
};

// Place and connect two diagrams in merge composition.
// The outputs of the first node are merged to the inputs of the second.
struct MergeNode : BinaryNode {
    MergeNode(Tree tree, Node *c1, Node *c2) : BinaryNode(tree, c1, c2) {}

    void DrawConnections(Device &device) const override {
        for (Count i = 0; i < IoCount(IO_Out, 0); i++) {
            device.Line(Node::Point(0, IO_Out, i), Node::Point(1, IO_In, i % IoCount(IO_In, 1)));
        }
    }
};

// Place and connect two diagrams in split composition.
// The outputs the first node are distributed to the inputs of the second.
struct SplitNode : BinaryNode {
    SplitNode(Tree tree, Node *c1, Node *c2) : BinaryNode(tree, c1, c2) {}

    void DrawConnections(Device &device) const override {
        for (Count i = 0; i < IoCount(IO_In, 1); i++) {
            device.Line(Node::Point(0, IO_Out, i % IoCount(IO_Out, 0)), Node::Point(1, IO_In, i));
        }
    }
};

Node *MakeSequential(Tree tree, Node *c1, Node *c2) {
    const auto o = c1->OutCount;
    const auto i = c2->InCount;
    return new SequentialNode(tree,
        o < i ? new ParallelNode(tree, c1, new CableNode(tree, i - o)) : c1,
        o > i ? new ParallelNode(tree, c2, new CableNode(tree, o - i)) : c2
    );
}

/**
Both `GroupNode` and `DecorateNode` render a grouping border around the provided `inner` node.

# Respected layout properties

Each property can be changed in `Style.FlowGrid.Diagram.(Group|Decorate){PropertyName}`.

* Margin (`Vec2`):
  - Adds to total size.
  - Offsets child position
  - Offsets grouping border
* Padding (`Vec2`):
  - Adds to total size.
  - Offsets child position (in addition to `Margin`)

# Render:

1) Border rectangle at `Margin` offset, with a break for a label in the top-left,
  and additional half-text-height Y-offset to center top border line with label.
  * Stylable fields:
    * Stroke width
    * Stroke color
2) Horizontal channel IO connection lines, at channel's vertical offset and from/to X:
  * Input:
     From: My left
     To: The left of my child at index `channel`
  * Output:
    * From: The right of my child at index `channel`
    * To: My right
*/
struct GroupNode : Node {
    GroupNode(Tree tree, Node *inner, string text = "", const string &label = "")
        : Node(tree, inner->InCount, inner->OutCount, std::move(text), {inner}), Label(label.empty() ? Text : label) {}

    void DoPlaceSize(const DeviceType) override { Size = C1()->Size + (Margin() + Padding()) * 2 + ImVec2{LineWidth() * 2, LineWidth() + GetFontSize()}; }
    void DoPlace(const DeviceType type) override { C1()->Place(type, Margin() + Padding() + ImVec2{LineWidth(), GetFontSize()}, Orientation); }
    void DoDraw(Device &device) const override {
        device.LabeledRect(
            {Margin() + LineWidth() / 2, Size - Margin() - LineWidth() / 2}, Label,
            {.StrokeColor=s.Style.FlowGrid.Diagram.Colors[FlowGridDiagramCol_GroupStroke], .StrokeWidth=s.Style.FlowGrid.Diagram.GroupLineWidth, .CornerRadius=s.Style.FlowGrid.Diagram.GroupCornerRadius},
            {.Color=s.Style.FlowGrid.Diagram.Colors[FlowGridDiagramCol_Text], .Padding={0, Device::RectLabelPaddingLeft}}
        );
    }

    // Y position of point is delegated to the grouped child.
    ImVec2 Point(IO io, Count channel) const override { return {Node::Point(io, channel).x, Node::Point(0, io, channel).y}; }

private:
    static float LineWidth() { return s.Style.FlowGrid.Diagram.GroupLineWidth; }
    ImVec2 Margin() const override { return s.Style.FlowGrid.Diagram.GroupMargin; }
    ImVec2 Padding() const override { return s.Style.FlowGrid.Diagram.GroupPadding; }

    void DrawConnections(Device &device) const override {
        const auto &offset = Margin() + Padding() + LineWidth();
        for (const IO io: IO_All) {
            const bool in = io == IO_In;
            for (Count channel = 0; channel < IoCount(io); channel++) {
                const auto &channel_point = Node::Point(0, io, channel);
                device.Line(
                    {in ? 0 : (Size - offset).x, channel_point.y},
                    {in ? offset.x : Size.x, channel_point.y}
                );
            }
        }
    }

    const string Label;
};

struct DecorateNode : Node {
    DecorateNode(Tree tree, Node *inner, string text = "", const string &label = "")
        : Node(tree, inner->InCount, inner->OutCount, std::move(text), {inner}), Label(label.empty() ? Text : label) {}

    void DoPlaceSize(const DeviceType) override {
        if (!ShouldDecorate()) Size = C1()->Size;
        Size = C1()->Size + (Margin() + Padding()) * 2 + ImVec2{LineWidth() * 2, LineWidth() + GetFontSize()};
    }
    void DoPlace(const DeviceType type) override {
        if (!ShouldDecorate()) return C1()->Place(type, {0, 0}, Orientation);
        C1()->Place(type, Margin() + Padding() + ImVec2{LineWidth(), GetFontSize()}, Orientation);
    }
    void DoDraw(Device &device) const override {
        if (!ShouldDecorate()) return;
        device.LabeledRect(
            {Margin() + LineWidth() / 2, Size - Margin() - LineWidth() / 2}, Label,
            {.StrokeColor=s.Style.FlowGrid.Diagram.Colors[FlowGridDiagramCol_DecorateStroke], .StrokeWidth=s.Style.FlowGrid.Diagram.DecorateLineWidth, .CornerRadius=s.Style.FlowGrid.Diagram.DecorateCornerRadius},
            {.Color=s.Style.FlowGrid.Diagram.Colors[FlowGridDiagramCol_Text], .Padding={0, Device::RectLabelPaddingLeft}}
        );
    }

private:
    static bool ShouldDecorate() { return s.Style.FlowGrid.Diagram.DecorateRootNode; }

    static float LineWidth() { return ShouldDecorate() ? s.Style.FlowGrid.Diagram.DecorateLineWidth : 0.f; }
    ImVec2 Margin() const override { return ShouldDecorate() ? s.Style.FlowGrid.Diagram.DecorateMargin : ImVec2{0, 0}; }
    ImVec2 Padding() const override { return ShouldDecorate() ? s.Style.FlowGrid.Diagram.DecoratePadding : ImVec2{0, 0}; }

    void DrawConnections(Device &device) const override {
        const auto &offset = Margin() + Padding() + LineWidth();
        for (const IO io: IO_All) {
            const bool in = io == IO_In;
            const float arrow_width = in ? 0.f : s.Style.FlowGrid.Diagram.ArrowSize.X;
            for (Count channel = 0; channel < IoCount(io); channel++) {
                const auto &channel_point = Point(0, io, channel);
                const ImVec2 &a = {in ? -offset.x : (Size - offset).x, channel_point.y};
                const ImVec2 &b = {in ? offset.x : Size.x - arrow_width, channel_point.y};
                if (ShouldDecorate()) device.Line(a, b);
                if (!in) device.Arrow(b + ImVec2{arrow_width, 0}, Orientation);
            }
        }
    }

private:
    const string Label;
};

struct RouteNode : Node {
    RouteNode(Tree tree, Count in_count, Count out_count, vector<int> routes)
        : Node(tree, in_count, out_count), routes(std::move(routes)) {}

    void DoPlaceSize(const DeviceType) override {
        const float minimal = 3 * WireGap();
        const float h = 2 * YMargin() + max(minimal, float(max(InCount, OutCount)) * WireGap());
        Size = {2 * XMargin() + max(minimal, h * 0.75f), h};
    }
    void DoPlace(const DeviceType) override {}

    void DoDraw(Device &device) const override {
        if (s.Style.FlowGrid.Diagram.RouteFrame) {
            device.Rect(GetFrameRect(), {.FillColor={0.93f, 0.93f, 0.65f, 1.f}}); // todo move to style
            DrawOrientationMark(device);
            // Input arrows
            for (Count i = 0; i < IoCount(IO_In); i++) device.Arrow(Point(IO_In, i) + ImVec2{DirUnit() * XMargin(), 0}, Orientation);
        }
    }
    void DrawConnections(Device &device) const override {
        const auto d = ImVec2{DirUnit() * XMargin(), 0};
        for (const IO io: IO_All) {
            const bool in = io == IO_In;
            for (Count i = 0; i < IoCount(io); i++) {
                const auto &p = Point(io, i);
                device.Line(in ? p : p - d, in ? p + d : p);
            }
        }
        for (Count i = 0; i < routes.size() - 1; i += 2) {
            const Count src = routes[i];
            const Count dst = routes[i + 1];
            if (src > 0 && src <= InCount && dst > 0 && dst <= OutCount) {
                device.Line(Point(IO_In, src - 1) + d, Point(IO_Out, dst - 1) - d);
            }
        }
    }

protected:
    const vector<int> routes; // Route description: c1,d2,c2,d2,...
};

static bool isBoxBinary(Box box, Box &x, Box &y) {
    return isBoxPar(box, x, y) || isBoxSeq(box, x, y) || isBoxSplit(box, x, y) || isBoxMerge(box, x, y) || isBoxRec(box, x, y);
}

// Returns `true` if `t == '*(-1)'`.
// This test is used to simplify diagram by using a special symbol for inverters.
static bool isBoxInverter(Box box) {
    static Tree inverters[6]{
        boxSeq(boxPar(boxWire(), boxInt(-1)), boxPrim2(sigMul)),
        boxSeq(boxPar(boxInt(-1), boxWire()), boxPrim2(sigMul)),
        boxSeq(boxPar(boxWire(), boxReal(-1.0)), boxPrim2(sigMul)),
        boxSeq(boxPar(boxReal(-1.0), boxWire()), boxPrim2(sigMul)),
        boxSeq(boxPar(boxInt(0), boxWire()), boxPrim2(sigSub)),
        boxSeq(boxPar(boxReal(0.0), boxWire()), boxPrim2(sigSub)),
    };
    return ::ranges::contains(inverters, box);
}

static inline string PrintTree(Tree tree) {
    const auto &str = printBox(tree, false);
    return str.substr(0, str.size() - 1); // Last character is a newline.
}

// Convert user interface box into a textual representation
static string GetUiDescription(Box box) {
    Tree t1, label, cur, min, max, step, chan;
    if (isBoxButton(box, label)) return "button(" + extractName(label) + ')';
    if (isBoxCheckbox(box, label)) return "checkbox(" + extractName(label) + ')';
    if (isBoxVSlider(box, label, cur, min, max, step)) return "vslider(" + extractName(label) + ", " + PrintTree(cur) + ", " + PrintTree(min) + ", " + PrintTree(max) + ", " + PrintTree(step) + ')';
    if (isBoxHSlider(box, label, cur, min, max, step)) return "hslider(" + extractName(label) + ", " + PrintTree(cur) + ", " + PrintTree(min) + ", " + PrintTree(max) + ", " + PrintTree(step) + ')';
    if (isBoxVGroup(box, label, t1)) return "vgroup(" + extractName(label) + ", " + PrintTree(t1) + ')';
    if (isBoxHGroup(box, label, t1)) return "hgroup(" + extractName(label) + ", " + PrintTree(t1) + ')';
    if (isBoxTGroup(box, label, t1)) return "tgroup(" + extractName(label) + ", " + PrintTree(t1) + ')';
    if (isBoxHBargraph(box, label, min, max)) return "hbargraph(" + extractName(label) + ", " + PrintTree(min) + ", " + PrintTree(max) + ')';
    if (isBoxVBargraph(box, label, min, max)) return "vbargraph(" + extractName(label) + ", " + PrintTree(min) + ", " + PrintTree(max) + ')';
    if (isBoxNumEntry(box, label, cur, min, max, step)) return "nentry(" + extractName(label) + ", " + PrintTree(cur) + ", " + PrintTree(min) + ", " + PrintTree(max) + ", " + PrintTree(step) + ')';
    if (isBoxSoundfile(box, label, chan)) return "soundfile(" + extractName(label) + ", " + PrintTree(chan) + ')';

    throw std::runtime_error("ERROR : unknown user interface element");
}

// Generate a 1->0 block node for an input slot.
static Node *MakeInputSlot(Tree tree) { return new BlockNode(tree, 1, 0, "", FlowGridDiagramCol_Slot); }

// Collect the leaf numbers `tree` into vector `v`.
// Return `true` if `tree` is a number or a parallel tree of numbers.
static bool isBoxInts(Box box, vector<int> &v) {
    int i;
    if (isBoxInt(box, &i)) {
        v.push_back(i);
        return true;
    }

    double r;
    if (isBoxReal(box, &r)) {
        v.push_back(int(r));
        return true;
    }

    Tree x, y;
    if (isBoxPar(box, x, y)) return isBoxInts(x, v) && isBoxInts(y, v);

    throw std::runtime_error("Not a valid list of numbers : " + PrintTree(box));
}

// Track trees only made of cut, wires, or slots ("pure routing" trees).
static map<Tree, bool> IsTreePureRouting{};
static bool IsPureRouting(Tree t) {
    if (IsTreePureRouting.contains(t)) return IsTreePureRouting[t];

    Tree x, y;
    if (isBoxCut(t) || isBoxWire(t) || isBoxInverter(t) || isBoxSlot(t) || (isBoxBinary(t, x, y) && IsPureRouting(x) && IsPureRouting(y))) {
        IsTreePureRouting.emplace(t, true);
        return true;
    }

    IsTreePureRouting.emplace(t, false);
    return false;
}

static Node *Tree2Node(Tree);

static std::optional<pair<Count, string>> GetBoxPrimCountAndName(Box box) {
    prim0 p0;
    if (isBoxPrim0(box, &p0)) return pair(0, prim0name(p0));
    prim1 p1;
    if (isBoxPrim1(box, &p1)) return pair(1, prim1name(p1));
    prim2 p2;
    if (isBoxPrim2(box, &p2)) return pair(2, prim2name(p2));
    prim3 p3;
    if (isBoxPrim3(box, &p3)) return pair(3, prim3name(p3));
    prim4 p4;
    if (isBoxPrim4(box, &p4)) return pair(4, prim4name(p4));
    prim5 p5;
    if (isBoxPrim5(box, &p5)) return pair(5, prim5name(p5));

    return {};
}

// Generate the inside node of a block diagram according to its type.
static Node *Tree2NodeInner(Tree t) {
    if (getUserData(t) != nullptr) return new BlockNode(t, xtendedArity(t), 1, xtendedName(t));
    if (isBoxInverter(t)) return new InverterNode(t);

    if (const auto prim_count_and_name = GetBoxPrimCountAndName(t)) {
        const auto &[prim_count, name] = *prim_count_and_name;
        return new BlockNode(t, prim_count, 1, name);
    }

    int i;
    double r;
    if (isBoxInt(t, &i) || isBoxReal(t, &r)) return new BlockNode(t, 0, 1, isBoxInt(t) ? to_string(i) : to_string(r), FlowGridDiagramCol_Number);
    if (isBoxWaveform(t)) return new BlockNode(t, 0, 2, "waveform{...}");
    if (isBoxWire(t)) return new CableNode(t);
    if (isBoxCut(t)) return new CutNode(t);

    Tree ff;
    if (isBoxFFun(t, ff)) return new BlockNode(t, ffarity(ff), 1, ffname(ff));

    Tree label, chan, type, name, file;
    if (isBoxFConst(t, type, name, file) || isBoxFVar(t, type, name, file)) return new BlockNode(t, 0, 1, tree2str(name));
    if (isBoxButton(t) || isBoxCheckbox(t) || isBoxVSlider(t) || isBoxHSlider(t) || isBoxNumEntry(t)) return new BlockNode(t, 0, 1, GetUiDescription(t), FlowGridDiagramCol_Ui);
    if (isBoxVBargraph(t) || isBoxHBargraph(t)) return new BlockNode(t, 1, 1, GetUiDescription(t), FlowGridDiagramCol_Ui);
    if (isBoxSoundfile(t, label, chan)) return new BlockNode(t, 2, 2 + tree2int(chan), GetUiDescription(t), FlowGridDiagramCol_Ui);

    Tree a, b;
    if (isBoxMetadata(t, a, b)) return Tree2Node(a);

    const bool is_vgroup = isBoxVGroup(t, label, a), is_hgroup = isBoxHGroup(t, label, a), is_tgroup = isBoxTGroup(t, label, a);
    if (is_vgroup || is_hgroup || is_tgroup) return new GroupNode(t, Tree2Node(a), "", format("{}group({})", is_vgroup ? "v" : is_hgroup ? "h" : "t", extractName(label)));

    if (isBoxSeq(t, a, b)) return MakeSequential(t, Tree2Node(a), Tree2Node(b));
    if (isBoxPar(t, a, b)) return new ParallelNode(t, Tree2Node(a), Tree2Node(b));
    if (isBoxSplit(t, a, b)) return new SplitNode(t, Tree2Node(a), Tree2Node(b));
    if (isBoxMerge(t, a, b)) return new MergeNode(t, Tree2Node(a), Tree2Node(b));
    if (isBoxRec(t, a, b)) return new RecursiveNode(t, Tree2Node(a), Tree2Node(b));

    if (isBoxSlot(t, &i)) return new BlockNode(t, 0, 1, "", FlowGridDiagramCol_Slot);

    if (isBoxSymbolic(t, a, b)) {
        // Generate an abstraction node by placing in sequence the input slots and the body.
        auto *input_slots = MakeInputSlot(a);
        Tree _a, _b;
        while (isBoxSymbolic(b, _a, _b)) {
            input_slots = new ParallelNode(b, input_slots, MakeInputSlot(_a));
            b = _b;
        }
        auto *abstraction = MakeSequential(b, input_slots, Tree2Node(b));
        return !GetTreeName(t).empty() ? abstraction : new GroupNode(t, abstraction, "Abstraction");
    }
    if (isBoxEnvironment(t)) return new BlockNode(t, 0, 0, "environment{...}");

    Tree route;
    if (isBoxRoute(t, a, b, route)) {
        int ins, outs;
        vector<int> routes;
        // Build n x m cable routing
        if (isBoxInt(a, &ins) && isBoxInt(b, &outs) && isBoxInts(route, routes)) return new RouteNode(t, ins, outs, routes);
        throw std::runtime_error("Invalid route expression : " + PrintTree(t));
    }

    throw std::runtime_error("ERROR in Tree2NodeInner, box expression not recognized: " + PrintTree(t));
}

// This method calls itself through `Tree2NodeInner`.
// (Keeping that bad name to remind me to clean this up, likely into a `Node` ctor.)
static Node *Tree2Node(Tree t) {
    auto *node = Tree2NodeInner(t);
    if (GetTreeName(t).empty()) return node; // Normal case

    // `DiagramFoldComplexity == 0` means no folding.
    if (s.Style.FlowGrid.Diagram.FoldComplexity != 0 && node->Descendents >= Count(s.Style.FlowGrid.Diagram.FoldComplexity)) {
        int ins, outs;
        getBoxType(t, &ins, &outs);
        return new BlockNode(t, ins, outs, "", FlowGridDiagramCol_Link, new DecorateNode(t, node));
    }
    return IsPureRouting(t) ? node : new GroupNode(t, node);
}

string GetBoxType(Box t) {
    if (getUserData(t) != nullptr) return format("{}({},{})", xtendedName(t), xtendedArity(t), 1);
    if (isBoxInverter(t)) return "Inverter";
    if (isBoxInt(t)) return "Int";
    if (isBoxReal(t)) return "Real";
    if (isBoxWaveform(t)) return "Waveform";
    if (isBoxWire(t)) return "Cable";
    if (isBoxCut(t)) return "Cut";

    if (const auto prim_count_and_name = GetBoxPrimCountAndName(t)) return (*prim_count_and_name).second;

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
        throw std::runtime_error("Invalid route expression : " + PrintTree(t));
    }

    return "";
}

static Node *CreateRootNode(Tree t) {
    auto *inner = Tree2NodeInner(t);
    return new DecorateNode(t, inner);
}

void OnBoxChange(Box box) {
    IsTreePureRouting.clear();
    FocusedNodeStack = {};
    if (box) {
        RootNode = CreateRootNode(box);
        FocusedNodeStack.push(RootNode);
    } else {
        RootNode = nullptr;
    }
}

void SaveBoxSvg(const string &path) {
    if (!RootNode) return;

    // Render SVG diagram(s)
    fs::remove_all(path);
    fs::create_directory(path);

    auto *node = CreateRootNode(RootNode->FaustTree); // Create a fresh mutable root node to place and render.
    node->PlaceSize(DeviceType_SVG);
    node->Place(DeviceType_SVG);
    WriteSvg(node, path);
}

static int FoldComplexity = 0; // Cache the most recently seen value and recompile when it changes.

void Audio::FaustState::FaustDiagram::Render() const {
    if (!RootNode) {
        // todo don't show empty menu bar in this case
        TextUnformatted("Enter a valid Faust program into the 'Faust editor' window to view its diagram."); // todo link to window?
        return;
    }

    if (BeginMenuBar()) {
        if (BeginMenu("File")) {
            fg::MenuItem(show_save_faust_svg_file_dialog{});
            EndMenu();
        }
        if (BeginMenu("View")) {
            Settings.HoverFlags.RenderMenu();
            EndMenu();
        }
        EndMenuBar();
    }

    if (FocusedNodeStack.empty()) return;

    if (s.Style.FlowGrid.Diagram.FoldComplexity != FoldComplexity) {
        FoldComplexity = s.Style.FlowGrid.Diagram.FoldComplexity;
        OnBoxChange(RootNode->FaustTree);
    }

    {
        // Nav menu
        const bool can_nav = FocusedNodeStack.size() > 1;
        if (!can_nav) BeginDisabled();
        if (Button("Top")) while (FocusedNodeStack.size() > 1) FocusedNodeStack.pop();
        SameLine();
        if (Button("Back")) FocusedNodeStack.pop();
        if (!can_nav) EndDisabled();
    }

    auto *focused = FocusedNodeStack.top();
    focused->PlaceSize(DeviceType_ImGui);
    focused->Place(DeviceType_ImGui);
    if (!s.Style.FlowGrid.Diagram.ScaleFillHeight) SetNextWindowContentSize(Scale(focused->Size));
    BeginChild("Faust diagram inner", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);
    GetCurrentWindow()->FontWindowScale = Scale(1);
    GetWindowDrawList()->AddRectFilled(GetWindowPos(), GetWindowPos() + GetWindowSize(), s.Style.FlowGrid.Diagram.Colors[FlowGridDiagramCol_Bg]);

    ImGuiDevice device;
    HoveredNode = nullptr;
    focused->MarkFrame();
    focused->Draw(device);
    if (HoveredNode) {
        const auto &flags = Settings.HoverFlags;
        // todo get abs pos by traversing through ancestors
        if (flags & FaustDiagramHoverFlags_ShowRect) HoveredNode->DrawRect(device);
        if (flags & FaustDiagramHoverFlags_ShowType) HoveredNode->DrawType(device);
        if (flags & FaustDiagramHoverFlags_ShowChannels) HoveredNode->DrawChannelLabels(device);
        if (flags & FaustDiagramHoverFlags_ShowChildChannels) HoveredNode->DrawChildChannelLabels(device);
    }

    EndChild();
}
