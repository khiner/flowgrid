#include <sstream>
#include <stack>
#include <unordered_map>

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/view/map.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/take_while.hpp>

#include "faust/dsp/libfaust-box.h"
#include "faust/dsp/libfaust-signal.h"

#include "../../App.h"
#include "../../Helper/File.h"
#include "../../Helper/String.h"
#include "../../Helper/basen.h"

using namespace ImGui;
using std::min, std::max;
using std::unordered_map;

enum DeviceType {
    DeviceType_ImGui,
    DeviceType_SVG
};
enum GraphOrientation {
    GraphForward,
    GraphReverse
};

static inline auto &Style() { return s.Style.FlowGrid.Graph; }

static inline float GetScale();
static inline ImVec2 Scale(const ImVec2 &p) { return p * GetScale(); }
static inline float Scale(const float f) { return f * GetScale(); }

static inline ImGuiDir GlobalDirection(GraphOrientation orientation) {
    ImGuiDir dir = Style().Direction;
    return (dir == ImGuiDir_Right && orientation == GraphForward) || (dir == ImGuiDir_Left && orientation == GraphReverse) ?
        ImGuiDir_Right :
        ImGuiDir_Left;
}

static inline bool IsLr(GraphOrientation orientation) { return GlobalDirection(orientation) == ImGuiDir_Right; }

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
    virtual void LabeledRect(const ImRect &, string_view label, const RectStyle &, const TextStyle &) = 0;

    virtual void Triangle(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImColor &color) = 0;
    virtual void Circle(const ImVec2 &pos, float radius, const ImColor &fill_color, const ImColor &stroke_color) = 0;
    virtual void Arrow(const ImVec2 &pos, GraphOrientation) = 0;
    virtual void Line(const ImVec2 &start, const ImVec2 &end) = 0;
    virtual void Text(const ImVec2 &pos, string_view text, const TextStyle &) = 0;
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
    static unordered_map<string, string> base64_for_font_name; // avoid recomputing
    string font_name = GetFontName();
    if (!base64_for_font_name.contains(font_name)) {
        const string ttf_contents = FileIO::read(GetFontPath());
        string ttf_base64;
        bn::encode_b64(ttf_contents.begin(), ttf_contents.end(), back_inserter(ttf_base64));
        base64_for_font_name[font_name] = ttf_base64;
    }
    return base64_for_font_name.at(font_name);
}

// todo: Fix rendering SVG with `DecorateRootNode = false` (and generally get it back to its former self).
struct SVGDevice : Device {
    SVGDevice(fs::path Directory, string FileName, ImVec2 size) : Directory(std::move(Directory)), FileName(std::move(FileName)) {
        const auto &[w, h] = Scale(size);
        Stream << format(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {} {}")", w, h);
        Stream << (Style().ScaleFillHeight ? R"( height="100%">)" : format(R"( width="{}" height="{}">)", w, h));

        // Embed the current font as a base64-encoded string.
        Stream << format(R"(
        <defs><style>
            @font-face{{
                font-family:"{}";
                src:url(data:application/font-woff;charset=utf-8;base64,{}) format("woff");
                font-weight:normal;font-style:normal;
            }}
        </style></defs>)",
                         GetFontName(), GetFontBase64());
    }

    ~SVGDevice() override {
        Stream << "</svg>\n";
        FileIO::write(Directory / FileName, Stream.str());
    }

    DeviceType Type() override { return DeviceType_SVG; }

    static string XmlSanitize(string copy) {
        static unordered_map<char, string> Replacements{{'<', "&lt;"}, {'>', "&gt;"}, {'\'', "&apos;"}, {'"', "&quot;"}, {'&', "&amp;"}};
        for (const auto &[ch, replacement] : Replacements) copy = StringHelper::Replace(copy, ch, replacement);
        return copy;
    }

    // Render an arrow. 'pos' is position of the arrow tip. half_sz.x is length from base to tip. half_sz.y is length on each side.
    static string ArrowPointingAt(const ImVec2 &pos, ImVec2 half_sz, GraphOrientation orientation, const ImColor &color) {
        const float d = IsLr(orientation) ? -1 : 1;
        return CreateTriangle(ImVec2{pos.x + d * half_sz.x, pos.y - d * half_sz.y}, ImVec2{pos.x + d * half_sz.x, pos.y + d * half_sz.y}, pos, color, color);
    }
    static string CreateTriangle(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImColor &fill_color, const ImColor &stroke_color) {
        return format(R"(<polygon fill="{}" stroke="{}" stroke-width=".5" points="{},{} {},{} {},{}"/>)", RgbColor(fill_color), RgbColor(stroke_color), p1.x, p1.y, p2.x, p2.y, p3.x, p3.y);
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
        Stream << format(R"(<rect x="{}" y="{}" width="{}" height="{}" rx="{}" style="stroke:{};stroke-width={};fill:{};"/>)", rect.Min.x, rect.Min.y, rect.GetWidth(), rect.GetHeight(), corner_radius, RgbColor(stroke_color), stroke_width, RgbColor(fill_color));
    }

    // Only SVG device has a rect-with-link method
    void Rect(const ImRect &local_rect, const RectStyle &style, string_view link) {
        if (!link.empty()) Stream << format(R"(<a href="{}">)", XmlSanitize(string(link)));
        Rect(local_rect, style);
        if (!link.empty()) Stream << "</a>";
    }

    // todo port ImGui implementation changes here, and use that one arg to make rounded rect path go clockwise (there is one).
    void LabeledRect(const ImRect &local_rect, string_view label, const RectStyle &rect_style, const TextStyle &text_style) override {
        const auto &rect = At(local_rect);
        const auto &tl = rect.Min;
        const auto &tr = rect.GetTR();
        const float label_offset = Scale(max(8.f, rect_style.CornerRadius) + text_style.Padding.Left);
        const float text_x = tl.x + label_offset;
        const ImVec2 &text_right = {min(text_x + CalcTextSize(string(label)).x, tr.x), tr.y};
        const float r = Scale(rect_style.CornerRadius);
        // Going counter-clockwise instead of clockwise, like in the ImGui implementation, since that's what paths expect for corner rounding to work.
        Stream << format(R"(<path d="m{},{} h{} a{},{} 0 00 {},{} v{} a{},{} 0 00 {},{} h{} a{},{} 0 00 {},{} v{} a{},{} 0 00 {},{} h{}" stroke-width="{}" stroke="{}" fill="none"/>)", text_x - Scale(text_style.Padding.Left), tl.y, Scale(text_style.Padding.Right - label_offset) + r, r, r, -r, r, // before text to top-left
                         rect.GetHeight() - 2 * r, r, r, r, r, // top-left to bottom-left
                         rect.GetWidth() - 2 * r, r, r, r, -r, // bottom-left to bottom-right
                         -(rect.GetHeight() - 2 * r), r, r, -r, -r, // bottom-right to top-right
                         -(tr.x - r - text_right.x), // top-right to after text
                         Scale(rect_style.StrokeWidth), RgbColor(rect_style.StrokeColor));
        Stream << format(R"(<text x="{}" y="{}" font-family="{}" font-size="{}" fill="{}" dominant-baseline="middle">{}</text>)", text_x, tl.y, GetFontName(), GetFontSize(), RgbColor(text_style.Color), XmlSanitize(string(label)));
    }

    void Triangle(const ImVec2 &p1, const ImVec2 &p2, const ImVec2 &p3, const ImColor &color) override {
        Stream << CreateTriangle(At(p1), At(p2), At(p3), {0.f, 0.f, 0.f, 0.f}, color);
    }

    void Circle(const ImVec2 &pos, float radius, const ImColor &fill_color, const ImColor &stroke_color) override {
        const auto [x, y] = At(pos);
        Stream << format(R"(<circle fill="{}" stroke="{}" stroke-width=".5" cx="{}" cy="{}" r="{}"/>)", RgbColor(fill_color), RgbColor(stroke_color), x, y, radius);
    }

    void Arrow(const ImVec2 &pos, GraphOrientation orientation) override {
        Stream << ArrowPointingAt(At(pos), Scale(Style().ArrowSize), orientation, Style().Colors[FlowGridGraphCol_Line]);
    }

    void Line(const ImVec2 &start, const ImVec2 &end) override {
        const string line_cap = start.x == end.x || start.y == end.y ? "butt" : "round";
        const auto &start_scaled = At(start);
        const auto &end_scaled = At(end);
        const ImColor &color = Style().Colors[FlowGridGraphCol_Line];
        const auto width = Scale(Style().WireWidth);
        Stream << format(R"(<line x1="{}" y1="{}" x2="{}" y2="{}"  style="stroke:{}; stroke-linecap:{}; stroke-width:{};"/>)", start_scaled.x, start_scaled.y, end_scaled.x, end_scaled.y, RgbColor(color), line_cap, width);
    }

    void Text(const ImVec2 &pos, string_view text, const TextStyle &style) override {
        const auto &[color, justify, padding, font_style] = style;
        const string anchor = justify.h == HJustify_Left ? "start" : (justify.h == HJustify_Middle ? "middle" : "end");
        const string font_style_formatted = font_style == TextStyle::FontStyle::Italic ? "italic" : "normal";
        const string font_weight = font_style == TextStyle::FontStyle::Bold ? "bold" : "normal";
        const auto &p = At(pos - ImVec2{style.Padding.Right, style.Padding.Bottom});
        Stream << format(R"(<text x="{}" y="{}" font-family="{}" font-style="{}" font-weight="{}" font-size="{}" text-anchor="{}" fill="{}" dominant-baseline="middle">{}</text>)", p.x, p.y, GetFontName(), font_style_formatted, font_weight, GetFontSize(), anchor, RgbColor(color), XmlSanitize(string(text)));
    }

    // Only SVG device has a text-with-link method
    void Text(const ImVec2 &pos, string_view str, const TextStyle &style, string_view link) {
        if (!link.empty()) Stream << format(R"(<a href="{}">)", XmlSanitize(string(link)));
        Text(pos, str, style);
        if (!link.empty()) Stream << "</a>";
    }

    void Dot(const ImVec2 &pos, const ImColor &fill_color) override {
        const auto &p = At(pos);
        const float radius = Scale(Style().OrientationMarkRadius);
        Stream << format(R"(<circle cx="{}" cy="{}" r="{}" fill="{}"/>)", p.x, p.y, radius, RgbColor(fill_color));
    }

    fs::path Directory;
    string FileName;

private:
    std::stringstream Stream;
};

struct ImGuiDevice : Device {
    ImGuiDevice() : Device(GetCursorScreenPos()), DC(GetCurrentWindow()->DC), DrawList(GetWindowDrawList()) {}

    DeviceType Type() override { return DeviceType_ImGui; }

    void SetCursorPos(const ImVec2 &scaled_cursor_pos) override {
        Device::SetCursorPos(scaled_cursor_pos);
        DC.CursorPos = At({0, 0});
    }
    void Rect(const ImRect &local_rect, const RectStyle &style) override {
        const auto &rect = At(local_rect);
        const auto &[fill_color, stroke_color, stroke_width, corner_radius] = style;
        if (fill_color.Value.w != 0) DrawList->AddRectFilled(rect.Min, rect.Max, fill_color, corner_radius);
        if (stroke_color.Value.w != 0) DrawList->AddRect(rect.Min, rect.Max, stroke_color, corner_radius);
    }

    void LabeledRect(const ImRect &local_rect, string_view label, const RectStyle &rect_style, const TextStyle &text_style) override {
        const auto &rect = At(local_rect);
        const auto &padding = text_style.Padding;
        const auto &padding_left = Scale(padding.Left), &padding_right = Scale(padding.Right);
        const float r = Scale(rect_style.CornerRadius);
        const float label_offset_x = max(Scale(8), r) + padding_left;
        const auto &ellipsified_label = Ellipsify(string(label), rect.GetWidth() - r - label_offset_x - padding_right);

        // Clockwise, starting to right of text
        const auto &a = rect.Min + ImVec2{0, GetFontSize() / 2}, &b = rect.Max;
        const auto &text_top_left = rect.Min + ImVec2{label_offset_x, 0};
        const auto &rect_start = a + ImVec2{label_offset_x, 0} + ImVec2{CalcTextSize(ellipsified_label).x + padding_left, 0};
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

    void Arrow(const ImVec2 &p, GraphOrientation orientation) override {
        RenderArrowPointingAt(DrawList, At(p) + ImVec2{0, 0.5f}, Scale(Style().ArrowSize), GlobalDirection(orientation), Style().Colors[FlowGridGraphCol_Line]);
    }

    // Basically `DrawList->AddLine(...)`, but avoiding extra vec2 math to cancel out the +0.5x ImGui adds to line points.
    void Line(const ImVec2 &start, const ImVec2 &end) override {
        static const auto offset = ImVec2{0.f, 0.5f};
        DrawList->PathLineTo(At(start) + offset);
        DrawList->PathLineTo(At(end) + offset);
        DrawList->PathStroke(Style().Colors[FlowGridGraphCol_Line], 0, Scale(Style().WireWidth));
    }

    void Text(const ImVec2 &p, string_view text, const TextStyle &style) override {
        const auto &[color, justify, padding, font_style] = style;
        const auto text_copy = string(text);
        const auto &size = CalcTextSize(text_copy);
        DrawList->AddText(
            At(p - ImVec2{padding.Right, padding.Bottom}) -
                ImVec2{
                    justify.h == HJustify_Left ? 0 : (justify.h == HJustify_Middle ? size.x / 2 : size.x),
                    justify.v == VJustify_Top ? 0 : (justify.v == VJustify_Middle ? size.y / 2 : size.y),
                },
            color, text_copy.c_str()
        );
    }

    void Dot(const ImVec2 &p, const ImColor &fill_color) override {
        const float radius = Scale(Style().OrientationMarkRadius);
        DrawList->AddCircleFilled(At(p), radius, fill_color);
    }

    ImGuiWindowTempData &DC; // Safe to store directly, since the device is recreated each frame.
    ImDrawList *DrawList;
};

static string GetTreeName(Tree tree) {
    Tree name;
    return getDefNameProperty(tree, name) ? tree2str(name) : "";
}

static string GetBoxType(Box t);

string GetTreeInfo(Tree tree) {
    return GetBoxType(tree);
}

// Hex address (without the '0x' prefix)
static string UniqueId(const void *instance) { return format("{:x}", reinterpret_cast<std::uintptr_t>(instance)); }

using StringHelper::Capitalize;

// An abstract block graph node.
struct Node {
    inline static unordered_map<ID, const Node *> WithId;

    const Tree FaustTree;
    const string Id, Text, BoxTypeLabel;
    const Count InCount, OutCount;
    const Count Descendents = 0; // The number of boxes within this node (recursively).
    Node *A{}, *B{}; // Nodes have at most two children.

    ImVec2 Size; // Set in `PlaceSize`.
    ImVec2 Position; // Relative to parent. Set in `Place`.
    GraphOrientation Orientation = GraphForward; // Set in `Place`.

    Node(Tree tree, Count in_count, Count out_count, Node *a = nullptr, Node *b = nullptr, string text = "", bool is_block = false)
        : FaustTree(tree), Id(UniqueId(FaustTree)), Text(!text.empty() ? std::move(text) : GetTreeName(FaustTree)),
          BoxTypeLabel(GetBoxType(FaustTree)), InCount(in_count), OutCount(out_count),
          Descendents((is_block ? 1 : 0) + (a ? a->Descendents : 0) + (b ? b->Descendents : 0)), A(a), B(b) {
        // cout << tree2str(tree) << '\n';
    }

    virtual ~Node() = default;

    void AddId(ID parent_id) const {
        const auto imgui_id = ImHashStr(Id.c_str(), 0, parent_id);
        WithId[imgui_id] = this;
        if (A) A->AddId(imgui_id);
        if (B) B->AddId(imgui_id);
    }

    Count IoCount(IO io) const { return io == IO_In ? InCount : OutCount; };

    // IO point relative to self.
    virtual ImVec2 Point(IO io, Count channel) const {
        return {
            ((io == IO_In && IsLr()) || (io == IO_Out && !IsLr()) ? 0 : W()),
            Size.y / 2 - WireGap() * (float(IoCount(io) - 1) / 2 - float(channel)) * OrientationUnit()};
    }
    // IO point relative to parent.
    ImVec2 ChildPoint(IO io, Count channel) const { return Position + Point(io, channel); }

    void Place(const DeviceType type, const ImVec2 &position, GraphOrientation orientation) {
        Position = position;
        Orientation = orientation;
        DoPlace(type);
    }
    void PlaceSize(const DeviceType type) {
        if (A) A->PlaceSize(type);
        if (B) B->PlaceSize(type);
        DoPlaceSize(type);
    }
    void Place(const DeviceType type) { DoPlace(type); }
    void Draw(Device &device) const {
        const bool is_imgui = device.Type() == DeviceType_ImGui;
        const auto before_cursor = device.CursorPosition;
        device.AdvanceCursor(Position);
        if (is_imgui) PushID(Id.c_str());

        InteractionFlags flags = InteractionFlags_None;
        if (is_imgui) {
            const auto before_cursor_inner = device.CursorPosition;
            const auto &local_rect = GetFrameRect();
            device.AdvanceCursor(local_rect.Min);
            flags |= fg::InvisibleButton(Scale(local_rect.GetSize()), "");
            SetItemAllowOverlap();
            device.SetCursorPos(before_cursor_inner);
        }

        Render(device, flags);
        if (A) A->Draw(device);
        if (B) B->Draw(device);

        if (flags & InteractionFlags_Hovered) {
            const auto &flags = s.Faust.Graph.Settings.HoverFlags;
            // todo get abs pos by traversing through ancestors
            if (flags & FaustGraphHoverFlags_ShowRect) DrawRect(device);
            if (flags & FaustGraphHoverFlags_ShowType) DrawType(device);
            if (flags & FaustGraphHoverFlags_ShowChannels) DrawChannelLabels(device);
            if (flags & FaustGraphHoverFlags_ShowChildChannels) DrawChildChannelLabels(device);
        }

        if (is_imgui) PopID();
        device.SetCursorPos(before_cursor);
    };

    inline static float WireGap() { return Style().WireGap; }

    virtual ImVec2 Margin() const { return Style().NodeMargin; }
    virtual ImVec2 Padding() const { return Style().NodePadding; } // Currently only actually used for `BlockNode` text
    inline float XMargin() const { return Margin().x; }
    inline float YMargin() const { return Margin().y; }

    inline float W() const { return Size.x; }
    inline float H() const { return Size.y; }
    inline operator ImRect() const { return {{0, 0}, Size}; }

    inline bool IsForward() const { return Orientation == GraphForward; }
    inline float OrientationUnit() const { return IsForward() ? 1 : -1; }

    inline bool IsLr() const { return ::IsLr(Orientation); }
    inline float DirUnit() const { return IsLr() ? 1 : -1; }
    inline float DirUnit(IO io) const { return DirUnit() * (io == IO_In ? 1.f : -1.f); }

    // Debug
    void DrawRect(Device &device) const {
        device.Rect(*this, {.FillColor = {0.5f, 0.5f, 0.5f, 0.1f}, .StrokeColor = {0.f, 0.f, 1.f, 1.f}, .StrokeWidth = 1});
    }
    void DrawType(Device &device) const {
        const static float padding = 2;
        const auto &label = format("{}: {}", BoxTypeLabel, Descendents);
        device.Rect({{0, 0}, CalcTextSize(label) + padding * 2}, {.FillColor = {0.5f, 0.5f, 0.5f, 0.3f}});
        device.Text({padding, padding}, label, {.Color = {1.f, 0.f, 0.f, 1.f}, .Justify = {HJustify_Left, VJustify_Top}});
    }
    void DrawChannelLabels(Device &device) const {
        for (const IO io : IO_All) {
            for (Count channel = 0; channel < IoCount(io); channel++) {
                device.Text(
                    Point(io, channel),
                    format("{}:{}", Capitalize(to_string(io, true)), channel),
                    {.Color = {0.f, 0.f, 1.f, 1.f}, .Justify = {HJustify_Right, VJustify_Middle}, .Padding = {6, 4}, .FontStyle = TextStyle::FontStyle::Bold}
                );
                device.Circle(Point(io, channel), 3, {0.f, 0.f, 1.f, 1.f}, {0.f, 0.f, 0.f, 1.f});
            }
        }
    }
    void DrawChildChannelLabels(Device &device) const {
        for (const IO io : IO_All) {
            for (Count child_index = 0; child_index < (B ? 2 : (A ? 1 : 0)); child_index++) {
                auto *child = child_index == 0 ? A : B;
                for (Count channel = 0; channel < child->IoCount(io); channel++) {
                    device.Text(
                        child->ChildPoint(io, channel),
                        format("C{}->{}:{}", child_index, Capitalize(to_string(io, true)), channel),
                        {.Color = {1.f, 0.f, 0.f, 1.f}, .Justify = {HJustify_Right, VJustify_Middle}, .Padding = {0, 4, 0, 0}, .FontStyle = TextStyle::FontStyle::Bold}
                    );
                    device.Circle(child->ChildPoint(io, channel), 2, {1.f, 0.f, 0.f, 1.f}, {0.f, 0.f, 0.f, 1.f});
                }
            }
        }
    }

    // Get a unique, length-limited, alphanumeric file name.
    // If this is not the (singular) process node, append its tree's hex address (without the '0x' prefix) to make the file name unique.
    string SvgFileName() const {
        if (!FaustTree) return "";

        const string tree_name = GetTreeName(FaustTree);
        if (tree_name == "process") return tree_name + ".svg";

        return (views::take_while(tree_name, [](char c) { return std::isalnum(c); }) | views::take(16) | to<string>)+format("-{}", Id) + ".svg";
    }

    void WriteSvg(const fs::path &path) const {
        SVGDevice device(path, SvgFileName(), Size);
        device.Rect(*this, {.FillColor = Style().Colors[FlowGridGraphCol_Bg]}); // todo this should be done in both cases
        Draw(device);
    }

protected:
    virtual void DoPlaceSize(DeviceType) = 0;
    virtual void DoPlace(DeviceType) = 0;
    virtual void Render(Device &, InteractionFlags flags = InteractionFlags_None) const = 0;

    ImRect GetFrameRect() const { return {Margin(), Size - Margin()}; }

    // Draw the orientation mark in the corner on the inputs side (respecting global direction setting), like in integrated circuits.
    // Marker on top: Forward orientation. Inputs go from top to bottom.
    // Marker on bottom: Backward orientation. Inputs go from bottom to top.
    void DrawOrientationMark(Device &device) const {
        if (!Style().OrientationMark) return;

        const auto &rect = GetFrameRect();
        const U32 color = Style().Colors[FlowGridGraphCol_OrientationMark];
        device.Dot(ImVec2{IsLr() ? rect.Min.x : rect.Max.x, IsForward() ? rect.Min.y : rect.Max.y} + ImVec2{DirUnit(), OrientationUnit()} * 4, color);
    }
};

std::stack<Node *> FocusedNodeStack;

static inline float GetScale() {
    if (!Style().ScaleFillHeight || FocusedNodeStack.empty() || !GetCurrentWindowRead()) return Style().Scale;
    return GetWindowHeight() / FocusedNodeStack.top()->H();
}

// A simple rectangular box with text and inputs/outputs.
struct BlockNode : Node {
    BlockNode(Tree tree, Count in_count, Count out_count, string text, FlowGridCol color = FlowGridGraphCol_Normal, Node *inner = nullptr)
        : Node(tree, in_count, out_count, nullptr, nullptr, std::move(text), true), Color(color), Inner(inner) {}

    void DoPlaceSize(const DeviceType type) override {
        Size = Margin() * 2 +
            ImVec2{
                max(3.f * WireGap(), CalcTextSize(string(Text)).x + Padding().x * 2),
                max(3.f * WireGap(), float(max(InCount, OutCount)) * WireGap()),
            };
        if (Inner && type == DeviceType_SVG) Inner->PlaceSize(type);
    }

    void DoPlace(const DeviceType type) override {
        if (Inner && type == DeviceType_SVG) Inner->Place(type);
    }

    void Render(Device &device, InteractionFlags flags) const override {
        U32 fill_color = Style().Colors[Color];
        const U32 text_color = Style().Colors[FlowGridGraphCol_Text];
        const auto &local_rect = GetFrameRect();
        const auto &size = local_rect.GetSize();
        const auto before_cursor = device.CursorPosition;
        device.AdvanceCursor(local_rect.Min); // todo this pattern should be RIAA style

        if (device.Type() == DeviceType_SVG) {
            auto &svg_device = dynamic_cast<SVGDevice &>(device);
            if (Inner && !fs::exists(svg_device.Directory / Inner->SvgFileName())) Inner->WriteSvg(svg_device.Directory);
            const string link = Inner ? SvgFileName() : "";
            svg_device.Rect({{0, 0}, size}, {.FillColor = fill_color, .CornerRadius = Style().BoxCornerRadius}, link);
            svg_device.Text(size / 2, Text, {.Color = text_color}, link);
        } else {
            if (Inner) {
                if (flags & InteractionFlags_Clicked) FocusedNodeStack.push(Inner);
                fill_color = GetColorU32(flags & InteractionFlags_Held ? ImGuiCol_ButtonActive : (flags & InteractionFlags_Hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
            }
            RenderFrame(device.At({0, 0}), device.At(size), fill_color, false, Style().BoxCornerRadius);
            device.Text(size / 2, Text, {.Color = text_color});
        }

        device.SetCursorPos(before_cursor);
        DrawOrientationMark(device);

        for (const IO io : IO_All) {
            const bool in = io == IO_In;
            const float arrow_width = in ? Style().ArrowSize.X : 0.f;
            for (Count channel = 0; channel < IoCount(io); channel++) {
                const auto &channel_point = Point(io, channel);
                const auto &b = channel_point + ImVec2{(XMargin() - arrow_width) * DirUnit(io), 0};
                device.Line(channel_point, b);
                if (in) device.Arrow(b + ImVec2{arrow_width * DirUnit(io), 0}, Orientation);
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
    void DoPlace(const DeviceType) override {}
    void Render(Device &, InteractionFlags) const override {}

    // Cable points are vertically spaced by `WireGap`.
    ImVec2 Point(IO, Count i) const override {
        const float dx = WireGap() * (float(i) + 0.5f);
        return {0, IsLr() ? dx : H() - dx};
    }
};

// An inverter is a circle followed by a triangle.
// It corresponds to '*(-1)', and it's used to create more compact graphs.
struct InverterNode : BlockNode {
    InverterNode(Tree tree) : BlockNode(tree, 1, 1, "-1", FlowGridGraphCol_Inverter) {}

    void DoPlaceSize(const DeviceType) override { Size = ImVec2{2.5f, 1} * WireGap(); }

    void Render(Device &device, InteractionFlags) const override {
        const float radius = Style().InverterRadius;
        const ImVec2 p1 = {W() - 2 * XMargin(), 1 + (H() - 1) / 2};
        const auto tri_a = ImVec2{XMargin() + (IsLr() ? 0 : p1.x), 0};
        const auto tri_b = tri_a + ImVec2{DirUnit() * (p1.x - 2 * radius) + (IsLr() ? 0 : W()), p1.y};
        const auto tri_c = tri_a + ImVec2{0, H()};
        device.Circle(tri_b + ImVec2{DirUnit() * radius, 0}, radius, {0.f, 0.f, 0.f, 0.f}, Style().Colors[Color]);
        device.Triangle(tri_a, tri_b, tri_c, Style().Colors[Color]);
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
    void Render(Device &, InteractionFlags) const override {
        // device.Circle(point, WireGap() / 8);
    }

    // A Cut has only one input point.
    ImVec2 Point(IO io, Count) const override {
        assert(io == IO_In);
        return {0, (Size / 2).y};
    }
};

// Parallel/Recursive nodes are split top/bottom.
// Sequential/Merge/Split nodes are split left/right.
enum BinaryNodeType {
    ParallelNode,
    RecursiveNode,
    SequentialNode,
    MergeNode,
    SplitNode,
};

struct BinaryNode : Node {
    BinaryNode(Tree tree, Node *a, Node *b, BinaryNodeType type)
        : Node(
              tree,
              type == ParallelNode ? a->InCount + b->InCount : (type == RecursiveNode ? a->InCount - b->OutCount : a->InCount),
              type == ParallelNode ? a->OutCount + b->OutCount : (type == RecursiveNode ? a->OutCount : b->OutCount),
              a, b
          ),
          Type(type) {}

    ImVec2 Point(IO io, Count i) const override {
        if (Type == ParallelNode) {
            const float dx = (io == IO_In ? -1.f : 1.f) * DirUnit();
            return i < A->IoCount(io) ?
                A->ChildPoint(io, i) + ImVec2{dx * (W() - A->W()) / 2, 0} :
                B->ChildPoint(io, i - A->IoCount(io)) + ImVec2{dx * (W() - B->W()) / 2, 0};
        }
        if (Type == RecursiveNode) {
            const bool lr = (io == IO_In && IsLr()) || (io == IO_Out && !IsLr());
            return {lr ? 0 : W(), A->ChildPoint(io, i + (io == IO_In ? B->IoCount(IO_Out) : 0)).y};
        }
        return (io == IO_In ? A : B)->ChildPoint(io, i);
    }
    void DoPlaceSize(const DeviceType) override {
        if (Type == ParallelNode) Size = {max(A->W(), B->W()), A->H() + B->H()};
        else if (Type == RecursiveNode) Size = {
                                            max(A->W(), B->W()) + 2 * WireGap() * float(max(B->IoCount(IO_In), B->IoCount(IO_Out))),
                                            A->H() + B->H()};
        else Size = {A->W() + B->W() + HorizontalGap(), max(A->H(), B->H())};
    }

    // Place the two components horizontally, centered, with enough space for the connections.
    void DoPlace(const DeviceType type) override {
        if (Type == ParallelNode || Type == RecursiveNode) {
            // For parallel, A is top and B is bottom. For recursive, this is reversed.
            // In both cases, flip the order if this node is oriented in reverse.
            const bool a_top = IsForward() == (Type == ParallelNode); // XNOR - result is true if either both are true or both are false.
            auto *top = a_top ? A : B;
            auto *bottom = a_top ? B : A;
            top->Place(type, {(W() - top->W()) / 2, 0}, Type == RecursiveNode ? GraphReverse : Orientation);
            bottom->Place(type, {(W() - bottom->W()) / 2, top->H()}, Type == RecursiveNode ? GraphForward : Orientation);
        } else {
            auto *left = IsLr() ? A : B;
            auto *right = IsLr() ? B : A;
            left->Place(type, {0, max(0.f, right->H() - left->H()) / 2}, Orientation);
            right->Place(type, {left->W() + HorizontalGap(), max(0.f, left->H() - right->H()) / 2}, Orientation);
        }
    }

    void Render(Device &device, InteractionFlags) const override {
        if (Type == ParallelNode) {
            for (const IO io : IO_All) {
                for (Count i = 0; i < IoCount(io); i++) {
                    device.Line(Point(io, i), i < A->IoCount(io) ? A->ChildPoint(io, i) : B->ChildPoint(io, i - A->IoCount(io)));
                }
            }
        } else if (Type == RecursiveNode) {
            assert(A->InCount >= B->OutCount);
            assert(A->OutCount >= B->InCount);
            const float dw = OrientationUnit() * WireGap();
            // out_a->in_b feedback connections
            for (Count i = 0; i < B->IoCount(IO_In); i++) {
                const auto &in_b = B->ChildPoint(IO_In, i);
                const auto &out_a = A->ChildPoint(IO_Out, i);
                const auto &from = ImVec2{IsLr() ? max(in_b.x, out_a.x) : min(in_b.x, out_a.x), out_a.y} + ImVec2{float(i) * dw, 0};
                // Draw the delay sign of a feedback connection (three sides of a square centered around the feedback source point).
                const auto &corner1 = from - ImVec2{dw, dw} / ImVec2{4, 2};
                const auto &corner2 = from + ImVec2{dw, -dw} / ImVec2{4, 2};
                device.Line(from - ImVec2{dw / 4, 0}, corner1);
                device.Line(corner1, corner2);
                device.Line(corner2, from + ImVec2{dw / 4, 0});
                // Draw the feedback line
                const ImVec2 &bend = {from.x, in_b.y};
                device.Line(from - ImVec2{0, dw / 2}, bend);
                device.Line(bend, in_b);
            }
            // Non-recursive output lines
            for (Count i = 0; i < OutCount; i++) device.Line(A->ChildPoint(IO_Out, i), Point(IO_Out, i));
            // Input lines
            for (Count i = 0; i < InCount; i++) device.Line(Point(IO_In, i), A->ChildPoint(IO_In, i + B->OutCount));
            // out_b->in_a feedfront connections
            for (Count i = 0; i < B->IoCount(IO_Out); i++) {
                const auto &from = B->ChildPoint(IO_Out, i);
                const auto &from_dx = from - ImVec2{dw * float(i), 0};
                const auto &to = A->ChildPoint(IO_In, i);
                const ImVec2 &corner1 = {to.x, from_dx.y};
                const ImVec2 &corner2 = {from_dx.x, to.y};
                const ImVec2 &bend = IsLr() ? (from_dx.x > to.x ? corner1 : corner2) : (from_dx.x > to.x ? corner2 : corner1);
                device.Line(from, from_dx);
                device.Line(from_dx, bend);
                device.Line(bend, to);
            }
        } else if (Type == SequentialNode) {
            assert(A->OutCount == B->InCount); // Children must be "compatible" (a: n->m and b: m->q).
            if (!Style().SequentialConnectionZigzag) {
                // Draw a straight, potentially diagonal cable.
                for (Count i = 0; i < A->IoCount(IO_Out); i++) device.Line(A->ChildPoint(IO_Out, i), B->ChildPoint(IO_In, i));
                return;
            }
            // todo should be able to simplify now and not create this map
            unordered_map<ImGuiDir, vector<Count>> ChannelsForDirection;
            for (Count i = 0; i < A->IoCount(IO_Out); i++) {
                const auto dy = B->ChildPoint(IO_In, i).y - A->ChildPoint(IO_Out, i).y;
                ChannelsForDirection[dy == 0 ? ImGuiDir_None : (dy < 0 ? ImGuiDir_Up : ImGuiDir_Down)].emplace_back(i);
            }
            // Draw upward zigzag cables, with the x turning point determined by the index of the connection in the group.
            for (const auto dir : views::keys(ChannelsForDirection)) {
                const auto &channels = ChannelsForDirection.at(dir);
                for (Count i = 0; i < channels.size(); i++) {
                    const auto channel = channels[i];
                    const auto from = A->ChildPoint(IO_Out, channel);
                    const auto to = B->ChildPoint(IO_In, channel);
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
        } else if (Type == MergeNode) {
            // The outputs of the first node are merged to the inputs of the second.
            for (Count i = 0; i < A->IoCount(IO_Out); i++) {
                device.Line(A->ChildPoint(IO_Out, i), B->ChildPoint(IO_In, i % B->IoCount(IO_In)));
            }
        } else if (Type == SplitNode) {
            // The outputs the first node are distributed to the inputs of the second.
            for (Count i = 0; i < B->IoCount(IO_In); i++) {
                device.Line(A->ChildPoint(IO_Out, i % A->IoCount(IO_Out)), B->ChildPoint(IO_In, i));
            }
        }
    }

    float HorizontalGap() const {
        if (Type == SequentialNode) {
            // The horizontal gap for the wires depends on the largest group of contiguous connections that go in the same up/down direction.
            if (A->IoCount(IO_Out) == 0) return 0;

            // todo simplify this by only tracking two counts: max same dir count in either direction, and current same dir count ...
            ImGuiDir prev_dir = ImGuiDir_None;
            Count same_dir_count = 0;
            unordered_map<ImGuiDir, Count> max_group_size; // Store the size of the largest group for each direction.
            for (Count i = 0; i < A->IoCount(IO_Out); i++) {
                const float yd = B->ChildPoint(IO_In, i).y - A->ChildPoint(IO_Out, i).y;
                const auto dir = yd < 0 ? ImGuiDir_Up : (yd > 0 ? ImGuiDir_Down : ImGuiDir_None);
                same_dir_count = dir == prev_dir ? same_dir_count + 1 : 1;
                prev_dir = dir;
                max_group_size[dir] = max(max_group_size[dir], same_dir_count);
            }

            return WireGap() * float(max(max_group_size[ImGuiDir_Up], max_group_size[ImGuiDir_Down]));
        }
        return (A->H() + B->H()) * Style().BinaryHorizontalGapRatio;
    }

    BinaryNodeType Type;
};

Node *MakeSequential(Tree tree, Node *a, Node *b) {
    const Count o = a->OutCount, i = b->InCount;
    return new BinaryNode(
        tree,
        o < i ? new BinaryNode(tree, a, new CableNode(tree, i - o), ParallelNode) : a,
        o > i ? new BinaryNode(tree, b, new CableNode(tree, o - i), ParallelNode) : b,
        SequentialNode
    );
}

enum NodeType {
    NodeType_Group,
    NodeType_Decorate,
};

/**
Both `GroupNode` and `DecorateNode` render a grouping border around the provided `inner` node.

# Respected layout properties

Each property can be changed in `Style.FlowGrid.Graph.(Group|Decorate){PropertyName}`.

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
    GroupNode(NodeType type, Tree tree, Node *inner, string text = "")
        : Node(tree, inner->InCount, inner->OutCount, inner, nullptr, std::move(text)), Type(type) {}

    void DoPlaceSize(const DeviceType) override {
        Size = A->Size + (Margin() + Padding()) * 2 + ImVec2{LineWidth() * 2, LineWidth() + GetFontSize()};
    }
    void DoPlace(const DeviceType type) override {
        if (!ShouldDecorate()) return A->Place(type, {0, 0}, Orientation);
        A->Place(type, Margin() + Padding() + ImVec2{LineWidth(), GetFontSize()}, Orientation);
    }
    void Render(Device &device, InteractionFlags) const override {
        if (ShouldDecorate()) {
            device.LabeledRect(
                {Margin() + LineWidth() / 2, Size - Margin() - LineWidth() / 2}, Text,
                {
                    .StrokeColor = Style().Colors[Type == NodeType_Group ? FlowGridGraphCol_GroupStroke : FlowGridGraphCol_DecorateStroke],
                    .StrokeWidth = Type == NodeType_Group ? Style().GroupLineWidth : Style().DecorateLineWidth,
                    .CornerRadius = Type == NodeType_Group ? Style().GroupCornerRadius : Style().DecorateCornerRadius,
                },
                {.Color = Style().Colors[FlowGridGraphCol_Text], .Padding = {0, Device::RectLabelPaddingLeft}}
            );
        }

        const auto &offset = Margin() + Padding() + LineWidth();
        for (const IO io : IO_All) {
            const bool in = io == IO_In;
            const float arrow_width = Type == NodeType_Group || in ? 0.f : Style().ArrowSize.X;
            for (Count channel = 0; channel < IoCount(io); channel++) {
                const auto &channel_point = A->ChildPoint(io, channel);
                const ImVec2 &a = {in ? -offset.x : (Size - offset).x, channel_point.y};
                const ImVec2 &b = {in ? offset.x : Size.x - arrow_width, channel_point.y};
                if (ShouldDecorate()) device.Line(a, b);
                if (Type == NodeType_Decorate && !in) device.Arrow(b + ImVec2{arrow_width, 0}, Orientation);
            }
        }
    }

    // Y position of point is delegated to the grouped child.
    ImVec2 Point(IO io, Count channel) const override { return {Node::Point(io, channel).x, A->ChildPoint(io, channel).y}; }

    NodeType Type;

private:
    inline bool ShouldDecorate() const { return Type == NodeType_Group || Style().DecorateRootNode; }
    inline float LineWidth() const { return !ShouldDecorate() ? 0.f : (Type == NodeType_Group ? Style().GroupLineWidth : Style().DecorateLineWidth); }
    ImVec2 Margin() const override { return !ShouldDecorate() ? ImVec2{0, 0} : (Type == NodeType_Group ? Style().GroupMargin : Style().DecorateMargin); }
    ImVec2 Padding() const override { return !ShouldDecorate() ? ImVec2{0, 0} : (Type == NodeType_Group ? Style().GroupPadding : Style().DecoratePadding); }
};

struct RouteNode : Node {
    RouteNode(Tree tree, Count in_count, Count out_count, vector<int> routes)
        : Node(tree, in_count, out_count), Routes(std::move(routes)) {}

    void DoPlaceSize(const DeviceType) override {
        const float minimal = 3 * WireGap();
        const float h = 2 * YMargin() + max(minimal, float(max(InCount, OutCount)) * WireGap());
        Size = {2 * XMargin() + max(minimal, h * 0.75f), h};
    }
    void DoPlace(const DeviceType) override {}

    void Render(Device &device, InteractionFlags) const override {
        if (Style().RouteFrame) {
            device.Rect(GetFrameRect(), {.FillColor = {0.93f, 0.93f, 0.65f, 1.f}}); // todo move to style
            DrawOrientationMark(device);
            // Input arrows
            for (Count i = 0; i < IoCount(IO_In); i++) device.Arrow(Point(IO_In, i) + ImVec2{DirUnit() * XMargin(), 0}, Orientation);
        }

        const auto d = ImVec2{DirUnit() * XMargin(), 0};
        for (const IO io : IO_All) {
            const bool in = io == IO_In;
            for (Count i = 0; i < IoCount(io); i++) {
                const auto &p = Point(io, i);
                device.Line(in ? p : p - d, in ? p + d : p);
            }
        }
        for (Count i = 0; i < Routes.size() - 1; i += 2) {
            const Count src = Routes[i];
            const Count dst = Routes[i + 1];
            if (src > 0 && src <= InCount && dst > 0 && dst <= OutCount) {
                device.Line(Point(IO_In, src - 1) + d, Point(IO_Out, dst - 1) - d);
            }
        }
    }

private:
    const vector<int> Routes; // Route description: a,d2,c2,d2,...
};

static bool isBoxBinary(Box box, Box &x, Box &y) {
    return isBoxPar(box, x, y) || isBoxSeq(box, x, y) || isBoxSplit(box, x, y) || isBoxMerge(box, x, y) || isBoxRec(box, x, y);
}

// Returns `true` if `t == '*(-1)'`.
// This test is used to simplify graph by using a special symbol for inverters.
static bool isBoxInverter(Box box) {
    static const Tree inverters[]{
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
    static const int max_num_characters = 20;
    const auto &str = printBox(tree, false, max_num_characters);
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
static Node *MakeInputSlot(Tree tree) { return new BlockNode(tree, 1, 0, "", FlowGridGraphCol_Slot); }

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
static unordered_map<Tree, bool> IsTreePureRouting{};
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

static Node *Tree2Node(Tree);

// Generate the inside node of a block graph according to its type.
static Node *Tree2NodeInner(Tree t) {
    if (getUserData(t) != nullptr) return new BlockNode(t, xtendedArity(t), 1, xtendedName(t));
    if (isBoxInverter(t)) return new InverterNode(t);
    if (isBoxButton(t) || isBoxCheckbox(t) || isBoxVSlider(t) || isBoxHSlider(t) || isBoxNumEntry(t)) return new BlockNode(t, 0, 1, GetUiDescription(t), FlowGridGraphCol_Ui);
    if (isBoxVBargraph(t) || isBoxHBargraph(t)) return new BlockNode(t, 1, 1, GetUiDescription(t), FlowGridGraphCol_Ui);
    if (isBoxWaveform(t)) return new BlockNode(t, 0, 2, "waveform{...}");
    if (isBoxWire(t)) return new CableNode(t);
    if (isBoxCut(t)) return new CutNode(t);
    if (isBoxEnvironment(t)) return new BlockNode(t, 0, 0, "environment{...}");
    if (const auto count_and_name = GetBoxPrimCountAndName(t)) return new BlockNode(t, (*count_and_name).first, 1, (*count_and_name).second);

    Tree a, b;
    if (isBoxMetadata(t, a, b)) return Tree2Node(a);
    if (isBoxSeq(t, a, b)) return MakeSequential(t, Tree2Node(a), Tree2Node(b));
    if (isBoxPar(t, a, b)) return new BinaryNode(t, Tree2Node(a), Tree2Node(b), ParallelNode);
    if (isBoxSplit(t, a, b)) return new BinaryNode(t, Tree2Node(a), Tree2Node(b), SplitNode);
    if (isBoxMerge(t, a, b)) return new BinaryNode(t, Tree2Node(a), Tree2Node(b), MergeNode);
    if (isBoxRec(t, a, b)) return new BinaryNode(t, Tree2Node(a), Tree2Node(b), RecursiveNode);
    if (isBoxSymbolic(t, a, b)) {
        // Generate an abstraction node by placing the input slots and body in sequence.
        auto *input_slots = MakeInputSlot(a);
        Tree _a, _b;
        while (isBoxSymbolic(b, _a, _b)) {
            input_slots = new BinaryNode(b, input_slots, MakeInputSlot(_a), ParallelNode);
            b = _b;
        }
        auto *abstraction = MakeSequential(b, input_slots, Tree2Node(b));
        return !GetTreeName(t).empty() ? abstraction : new GroupNode(NodeType_Group, t, abstraction, "Abstraction");
    }

    int i;
    double r;
    if (isBoxInt(t, &i) || isBoxReal(t, &r)) return new BlockNode(t, 0, 1, isBoxInt(t) ? to_string(i) : to_string(r), FlowGridGraphCol_Number);
    if (isBoxSlot(t, &i)) return new BlockNode(t, 0, 1, "", FlowGridGraphCol_Slot);

    Tree ff;
    if (isBoxFFun(t, ff)) return new BlockNode(t, ffarity(ff), 1, ffname(ff));

    Tree type, name, file;
    if (isBoxFConst(t, type, name, file) || isBoxFVar(t, type, name, file)) return new BlockNode(t, 0, 1, tree2str(name));

    Tree label, chan;
    if (isBoxSoundfile(t, label, chan)) return new BlockNode(t, 2, 2 + tree2int(chan), GetUiDescription(t), FlowGridGraphCol_Ui);

    const bool is_vgroup = isBoxVGroup(t, label, a), is_hgroup = isBoxHGroup(t, label, a), is_tgroup = isBoxTGroup(t, label, a);
    if (is_vgroup || is_hgroup || is_tgroup) {
        const char prefix = is_vgroup ? 'v' : (is_hgroup ? 'h' : 't');
        return new GroupNode(NodeType_Group, t, Tree2Node(a), format("{}group({})", prefix, extractName(label)));
    }

    Tree route;
    if (isBoxRoute(t, a, b, route)) {
        int ins, outs;
        vector<int> routes;
        // Build `ins`x`outs` cable routing.
        if (isBoxInt(a, &ins) && isBoxInt(b, &outs) && isBoxInts(route, routes)) return new RouteNode(t, ins, outs, routes);
        throw std::runtime_error("Invalid route expression : " + PrintTree(t));
    }

    throw std::runtime_error("Box expression not recognized: " + PrintTree(t));
}

static Count FoldComplexity = 0; // Cache the most recently seen value and recompile when it changes.

// This method calls itself through `Tree2NodeInner`.
// (Keeping these bad names to remind me to clean this up, likely into a `Node` ctor.)
static Node *Tree2Node(Tree t) {
    auto *node = Tree2NodeInner(t);
    if (GetTreeName(t).empty()) return node; // Normal case

    // `FoldComplexity == 0` means no folding.
    if (FoldComplexity != 0 && node->Descendents >= FoldComplexity) {
        int ins, outs;
        getBoxType(t, &ins, &outs);
        return new BlockNode(t, ins, outs, "", FlowGridGraphCol_Link, new GroupNode(NodeType_Decorate, t, node));
    }
    return IsPureRouting(t) ? node : new GroupNode(NodeType_Group, t, node);
}

string GetBoxType(Box t) {
    if (getUserData(t) != nullptr) return format("{}({},{})", xtendedName(t), xtendedArity(t), 1);
    if (isBoxInverter(t)) return "Inverter";
    if (isBoxInt(t)) return "Int";
    if (isBoxReal(t)) return "Real";
    if (isBoxWaveform(t)) return "Waveform";
    if (isBoxWire(t)) return "Cable";
    if (isBoxCut(t)) return "Cut";
    if (isBoxButton(t)) return "Button";
    if (isBoxCheckbox(t)) return "Checkbox";
    if (isBoxVSlider(t)) return "VSlider";
    if (isBoxHSlider(t)) return "HSlider";
    if (isBoxNumEntry(t)) return "NumEntry";
    if (isBoxVBargraph(t)) return "VBarGraph";
    if (isBoxHBargraph(t)) return "HBarGraph";
    if (isBoxVGroup(t)) return "VGroup";
    if (isBoxHGroup(t)) return "HGroup";
    if (isBoxTGroup(t)) return "TGroup";
    if (isBoxEnvironment(t)) return "Environment";
    if (const auto count_and_name = GetBoxPrimCountAndName(t)) return (*count_and_name).second;

    Tree a, b;
    if (isBoxSeq(t, a, b)) return "Sequential";
    if (isBoxPar(t, a, b)) return "Parallel";
    if (isBoxSplit(t, a, b)) return "Split";
    if (isBoxMerge(t, a, b)) return "Merge";
    if (isBoxRec(t, a, b)) return "Recursive";

    Tree ff;
    if (isBoxFFun(t, ff)) return format("FFun:{}({})", ffname(ff), ffarity(ff));

    Tree type, name, file;
    if (isBoxFConst(t, type, name, file)) return format("FConst:{}", tree2str(name));
    if (isBoxFVar(t, type, name, file)) return format("FVar:{}", tree2str(name));

    Tree label, chan;
    if (isBoxSoundfile(t, label, chan)) return format("Soundfile({},{})", 2, 2 + tree2int(chan));

    int i;
    if (isBoxSlot(t, &i)) return format("Slot({})", i);

    Tree route;
    if (isBoxRoute(t, a, b, route)) {
        int ins, outs;
        if (isBoxInt(a, &ins) && isBoxInt(b, &outs)) return format("Route({}x{})", ins, outs);
        throw std::runtime_error("Invalid route expression : " + PrintTree(t));
    }

    return "Unknown type";
}

static std::optional<GroupNode> RootNode{}; // This node is drawn every frame if present.
static GroupNode CreateRootNode(Tree t) { return {NodeType_Decorate, t, Tree2NodeInner(t)}; }

void OnBoxChange(Box box) {
    IsTreePureRouting.clear();
    FocusedNodeStack = {};
    if (box) {
        RootNode.emplace(CreateRootNode(box));
        FocusedNodeStack.push(&(*RootNode));
        Node::WithId.clear();
    } else {
        RootNode = std::nullopt;
    }
}

void SaveBoxSvg(string_view path) {
    if (!RootNode) return;

    fs::remove_all(path);
    fs::create_directory(path);

    auto node = CreateRootNode(RootNode->FaustTree); // Create a fresh mutable root node to place and render.
    node.PlaceSize(DeviceType_SVG);
    node.Place(DeviceType_SVG);
    node.WriteSvg(path);
}

Box GetHoveredBox(ID imgui_id) {
    const Node *node = Node::WithId[imgui_id];
    return node ? node->FaustTree : nullptr;
}

void Faust::FaustGraph::Render() const {
    if (!RootNode) {
        // todo don't show empty menu bar in this case
        TextUnformatted("Enter a valid Faust program into the 'Faust editor' window to view its graph."); // todo link to window?
        return;
    }

    if (FocusedNodeStack.empty()) return;

    if (Style().FoldComplexity != FoldComplexity) {
        FoldComplexity = Style().FoldComplexity;
        OnBoxChange(RootNode->FaustTree);
    }

    {
        // Nav menu
        const bool can_nav = FocusedNodeStack.size() > 1;
        if (!can_nav) BeginDisabled();
        if (Button("Top")) {
            while (FocusedNodeStack.size() > 1) FocusedNodeStack.pop();
        }
        SameLine();
        if (Button("Back")) FocusedNodeStack.pop();
        if (!can_nav) EndDisabled();
    }

    auto *focused = FocusedNodeStack.top();
    // auto start = Clock::now();
    focused->PlaceSize(DeviceType_ImGui);
    focused->Place(DeviceType_ImGui);
    // cout << "Place: " << FormatTimeSince(start) << '\n';

    if (!Style().ScaleFillHeight) SetNextWindowContentSize(Scale(focused->Size));
    BeginChild("Faust graph inner", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);
    if (Node::WithId.empty()) RootNode->AddId(GetCurrentWindowRead()->ID);
    GetCurrentWindow()->FontWindowScale = Scale(1);
    GetWindowDrawList()->AddRectFilled(GetWindowPos(), GetWindowPos() + GetWindowSize(), Style().Colors[FlowGridGraphCol_Bg]);

    ImGuiDevice device;
    // start = Clock::now();
    focused->Draw(device);
    // cout << "Draw: " << FormatTimeSince(start) << '\n';

    EndChild();
}
