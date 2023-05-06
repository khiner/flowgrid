#pragma once

#include "nlohmann/json_fwd.hpp"
#include <format>

#include "WindowMember.h"

#include "Audio.h"

/**
 * This class defines the main `State`, which fully describes the application at any point in time.
 * An immutable reference to the single source-of-truth application state `const State &s` is defined at the bottom of this file.
 */

namespace FlowGrid {}
namespace fg = FlowGrid;

using namespace nlohmann;
using action::ActionMoment, action::Gesture, action::Gestures, action::StateActionMoment;
using std::pair, std::make_unique, std::unique_ptr, std::unordered_map;

struct ImVec2;
struct ImVec4;
using ImGuiID = unsigned int;
using ImGuiTableFlags = int;
using ImGuiSliderFlags = int;

// Copy of some of ImGui's flags, to avoid including `imgui.h` in this header.
// Be sure to keep these in sync, because they are used directly as values for their ImGui counterparts.
enum Dir_ {
    Dir_None = -1,
    Dir_Left = 0,
    Dir_Right = 1,
    Dir_Up = 2,
    Dir_Down = 3,
    Dir_COUNT
};

enum SliderFlags_ {
    SliderFlags_None = 0,
    SliderFlags_AlwaysClamp = 1 << 4, // Clamp value to min/max bounds when input manually with CTRL+Click. By default CTRL+Click allows going out of bounds.
    SliderFlags_Logarithmic = 1 << 5, // Make the widget logarithmic (linear otherwise). Consider using ImGuiSliderFlags_NoRoundToFormat with this if using a format-string with small amount of digits.
};

// Subset of `ImGuiTableFlags`.
// Unlike the enums above, this one is not a copy of an ImGui enum.
// They can be converted between each other with `TableFlagsToImGui`.
// todo 'Condensed' preset, with NoHostExtendX, NoBordersInBody, NoPadOuterX
enum TableFlags_ {
    TableFlags_None = 0,
    // Features
    TableFlags_Resizable = 1 << 0,
    TableFlags_Reorderable = 1 << 1,
    TableFlags_Hideable = 1 << 2,
    TableFlags_Sortable = 1 << 3,
    TableFlags_ContextMenuInBody = 1 << 4,
    // Borders
    TableFlags_BordersInnerH = 1 << 5,
    TableFlags_BordersOuterH = 1 << 6,
    TableFlags_BordersInnerV = 1 << 7,
    TableFlags_BordersOuterV = 1 << 8,
    TableFlags_Borders = TableFlags_BordersInnerH | TableFlags_BordersOuterH | TableFlags_BordersInnerV | TableFlags_BordersOuterV,
    TableFlags_NoBordersInBody = 1 << 9,
    // Padding
    TableFlags_PadOuterX = 1 << 10,
    TableFlags_NoPadOuterX = 1 << 11,
    TableFlags_NoPadInnerX = 1 << 12,
};

using TableFlags = int;

ImGuiTableFlags TableFlagsToImGui(TableFlags);

struct Colors : UIStateMember {
    // An arbitrary transparent color is used to mark colors as "auto".
    // Using a the unique bit pattern `010101` for the RGB components so as not to confuse it with black/white-transparent.
    // Similar to ImPlot's usage of [`IMPLOT_AUTO_COL = ImVec4(0,0,0,-1)`](https://github.com/epezent/implot/blob/master/implot.h#L67).
    static constexpr U32 AutoColor = 0X00010101;

    Colors(StateMember *parent, string_view path_segment, string_view name_help, Count size, std::function<const char *(int)> get_color_name, const bool allow_auto = false);
    ~Colors();

    static U32 ConvertFloat4ToU32(const ImVec4 &value);
    static ImVec4 ConvertU32ToFloat4(const U32 value);

    Count Size() const;
    U32 operator[](Count) const;

    void Set(const vector<ImVec4> &, TransientStore &) const;
    void Set(const vector<pair<int, ImVec4>> &, TransientStore &) const;

protected:
    void Render() const override;

private:
    inline const UInt *At(Count) const;

    bool AllowAuto;
};

inline static const vector<Flags::Item> TableFlagItems{
    "Resizable?Enable resizing columns",
    "Reorderable?Enable reordering columns in header row",
    "Hideable?Enable hiding/disabling columns in context menu",
    "Sortable?Enable sorting",
    "ContextMenuInBody?Right-click on columns body/contents will display table context menu. By default it is available in headers row.",
    "BordersInnerH?Draw horizontal borders between rows",
    "BordersOuterH?Draw horizontal borders at the top and bottom",
    "BordersInnerV?Draw vertical borders between columns",
    "BordersOuterV?Draw vertical borders on the left and right sides",
    "NoBordersInBody?Disable vertical borders in columns Body (borders will always appear in Headers)",
    "PadOuterX?Default if 'BordersOuterV' is on. Enable outermost padding. Generally desirable if you have headers.",
    "NoPadOuterX?Default if 'BordersOuterV' is off. Disable outermost padding.",
    "NoPadInnerX?Disable inner padding between columns (double inner padding if 'BordersOuterV' is on, single inner padding if 'BordersOuterV' is off)",
};

WindowMember(
    ApplicationSettings,
    Prop(Float, GestureDurationSec, 0.5, 0, 5); // Merge actions occurring in short succession into a single gesture
);

WindowMember_(
    StateViewer,
    Menu({
        Menu("Settings", {AutoSelect, LabelMode}),
        Menu({}), // Need multiple elements to disambiguate vector-of-variants construction from variant construction.
    }),
    enum LabelMode{Annotated, Raw};
    Prop_(Enum, LabelMode, "?The raw JSON state doesn't store keys for all items.\n"
                           "For example, the main `ui.style.colors` state is a list.\n\n"
                           "'Annotated' mode shows (highlighted) labels for such state items.\n"
                           "'Raw' mode shows the state exactly as it is in the raw JSON state.",
          {"Annotated", "Raw"}, Annotated);
    Prop_(Bool, AutoSelect, "Auto-Select?When auto-select is enabled, state changes automatically open.\n"
                            "The state viewer to the changed state node(s), closing all other state nodes.\n"
                            "State menu items can only be opened or closed manually if auto-select is disabled.",
          true);

    void StateJsonTree(string_view key, const json &value, const StatePath &path = RootPath) const;
);

WindowMember_(StateMemoryEditor, WindowFlags_NoScrollbar);
WindowMember(StatePathUpdateFrequency);

WindowMember(
    ProjectPreview,
    Prop(Enum, Format, {"StateFormat", "ActionFormat"}, 1);
    Prop(Bool, Raw)
);

enum FlowGridCol_ {
    FlowGridCol_GestureIndicator, // 2nd series in ImPlot color map (same in all 3 styles for now): `ImPlot::GetColormapColor(1, 0)`
    FlowGridCol_HighlightText, // ImGuiCol_PlotHistogramHovered
    // Params colors.
    FlowGridCol_ParamsBg, // ImGuiCol_FrameBg with less alpha

    FlowGridCol_COUNT
};
using FlowGridCol = int;

enum FlowGridGraphCol_ {
    FlowGridGraphCol_Bg, // ImGuiCol_WindowBg
    FlowGridGraphCol_Text, // ImGuiCol_Text
    FlowGridGraphCol_DecorateStroke, // ImGuiCol_Border
    FlowGridGraphCol_GroupStroke, // ImGuiCol_Border
    FlowGridGraphCol_Line, // ImGuiCol_PlotLines
    FlowGridGraphCol_Link, // ImGuiCol_Button
    FlowGridGraphCol_Inverter, // ImGuiCol_Text
    FlowGridGraphCol_OrientationMark, // ImGuiCol_Text
    // Box fill colors of various types. todo design these colors for Dark/Classic/Light profiles
    FlowGridGraphCol_Normal,
    FlowGridGraphCol_Ui,
    FlowGridGraphCol_Slot,
    FlowGridGraphCol_Number,

    FlowGridGraphCol_COUNT
};
using FlowGridGraphCol = int;

struct Vec2 : UIStateMember {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Vec2(StateMember *parent, string_view path_segment, string_view name_help, const pair<float, float> &value = {0, 0}, float min = 0, float max = 1, const char *fmt = nullptr);

    operator ImVec2() const;

    Float X, Y;
    const char *Format;

protected:
    virtual void Render(ImGuiSliderFlags) const;
    void Render() const override;
};

struct Vec2Linked : Vec2 {
    using Vec2::Vec2;
    Vec2Linked(StateMember *parent, string_view path_segment, string_view name_help, const pair<float, float> &value = {0, 0}, float min = 0, float max = 1, bool linked = true, const char *fmt = nullptr);

    Prop(Bool, Linked, true);

protected:
    void Render(ImGuiSliderFlags) const override;
    void Render() const override;
};

struct Demo : TabsWindow {
    Demo(StateMember *parent, string_view path_segment, string_view name_help);

    UIMember(ImGuiDemo);
    UIMember(ImPlotDemo);
    UIMember(FileDialogDemo);

    Prop(ImGuiDemo, ImGui);
    Prop(ImPlotDemo, ImPlot);
    Prop(FileDialogDemo, FileDialog);
};

struct Metrics : TabsWindow {
    using TabsWindow::TabsWindow;

    UIMember(FlowGridMetrics, Prop(Bool, ShowRelativePaths, true));
    UIMember(ImGuiMetrics);
    UIMember(ImPlotMetrics);

    Prop(FlowGridMetrics, FlowGrid);
    Prop(ImGuiMetrics, ImGui);
    Prop(ImPlotMetrics, ImPlot);
};

#include "UI/Style.h"
#include "UI/UI.h"

enum InteractionFlags_ {
    InteractionFlags_None = 0,
    InteractionFlags_Hovered = 1 << 0,
    InteractionFlags_Held = 1 << 1,
    InteractionFlags_Clicked = 1 << 2,
};
using InteractionFlags = int;

// Namespace needed because Audio imports `CoreAudio.h`, which imports `CoreAudioTypes->MacTypes`, which has a `Style` type without a namespace.
namespace FlowGrid {
void HelpMarker(const char *help); // Like the one defined in `imgui_demo.cpp`

InteractionFlags InvisibleButton(const ImVec2 &size_arg, const char *id); // Basically `ImGui::InvisibleButton`, but supporting hover/held testing.

enum JsonTreeNodeFlags_ {
    JsonTreeNodeFlags_None = 0,
    JsonTreeNodeFlags_Highlighted = 1 << 0,
    JsonTreeNodeFlags_Disabled = 1 << 1,
    JsonTreeNodeFlags_DefaultOpen = 1 << 2,
};
using JsonTreeNodeFlags = int;

bool JsonTreeNode(string_view label, JsonTreeNodeFlags flags = JsonTreeNodeFlags_None, const char *id = nullptr);

// If `label` is empty, `JsonTree` will simply show the provided json `value` (object/array/raw value), with no nesting.
// For a non-empty `label`:
//   * If the provided `value` is an array or object, it will show as a nested `JsonTreeNode` with `label` as its parent.
//   * If the provided `value` is a raw value (or null), it will show as as '{label}: {value}'.
void JsonTree(string_view label, const json &value, JsonTreeNodeFlags node_flags = JsonTreeNodeFlags_None, const char *id = nullptr);

struct Style : TabsWindow {
    using TabsWindow::TabsWindow;

    UIMember_(
        FlowGridStyle,

        UIMember(
            Matrix,

            Prop_(Float, CellSize, "?The size of each matrix cell, as a multiple of line height.", 1, 1, 3);
            Prop_(Float, CellGap, "?The gap between matrix cells.", 1, 0, 10);
            Prop_(Float, LabelSize, "?The space provided for the label, as a multiple of line height.\n(Use Style->ImGui->InnerItemSpacing->X for spacing between labels and cells.)", 6, 3, 8);
        );

        UIMember_(
            Graph,

            Prop_(
                UInt, FoldComplexity,
                "?Number of boxes within a graph before folding into a sub-graph.\n"
                "Setting to zero disables folding altogether, for a fully-expanded graph.",
                3, 0, 20
            );
            Prop_(Bool, ScaleFillHeight, "?Automatically scale to fill the full height of the graph window, keeping the same aspect ratio.");
            Prop(Float, Scale, 1, 0.1, 5);
            Prop(Enum, Direction, {"Left", "Right"}, Dir_Right);
            Prop(Bool, RouteFrame);
            Prop(Bool, SequentialConnectionZigzag); // `false` uses diagonal lines instead of zigzags instead of zigzags
            Prop(Bool, OrientationMark);
            Prop(Float, OrientationMarkRadius, 1.5, 0.5, 3);

            Prop(Bool, DecorateRootNode, true);
            Prop(Vec2Linked, DecorateMargin, {10, 10}, 0, 20);
            Prop(Vec2Linked, DecoratePadding, {10, 10}, 0, 20);
            Prop(Float, DecorateLineWidth, 1, 1, 4);
            Prop(Float, DecorateCornerRadius, 0, 0, 10);

            Prop(Vec2Linked, GroupMargin, {8, 8}, 0, 20);
            Prop(Vec2Linked, GroupPadding, {8, 8}, 0, 20);
            Prop(Float, GroupLineWidth, 2, 1, 4);
            Prop(Float, GroupCornerRadius, 5, 0, 10);

            Prop(Vec2Linked, NodeMargin, {8, 8}, 0, 20);
            Prop(Vec2Linked, NodePadding, {8, 0}, 0, 20, false); // todo padding y not actually used yet, since blocks already have a min-height determined by WireGap.

            Prop(Float, BoxCornerRadius, 4, 0, 10);
            Prop(Float, BinaryHorizontalGapRatio, 0.25, 0, 1);
            Prop(Float, WireWidth, 1, 0.5, 4);
            Prop(Float, WireGap, 16, 10, 20);
            Prop(Vec2, ArrowSize, {3, 2}, 1, 10);
            Prop(Float, InverterRadius, 3, 1, 5);
            Prop(Colors, Colors, FlowGridGraphCol_COUNT, GetColorName);

            const vector<std::reference_wrapper<PrimitiveBase>> LayoutFields{
                SequentialConnectionZigzag,
                OrientationMark,
                OrientationMarkRadius,
                DecorateRootNode,
                DecorateMargin.X,
                DecorateMargin.Y,
                DecoratePadding.X,
                DecoratePadding.Y,
                DecorateLineWidth,
                DecorateCornerRadius,
                GroupMargin.X,
                GroupMargin.Y,
                GroupPadding.X,
                GroupPadding.Y,
                GroupLineWidth,
                GroupCornerRadius,
                BoxCornerRadius,
                BinaryHorizontalGapRatio,
                WireWidth,
                WireGap,
                NodeMargin.X,
                NodeMargin.Y,
                NodePadding.X,
                NodePadding.Y,
                ArrowSize.X,
                ArrowSize.Y,
                InverterRadius,
            };

            void ColorsDark(TransientStore &store) const;
            void ColorsClassic(TransientStore &store) const;
            void ColorsLight(TransientStore &store) const;
            void ColorsFaust(TransientStore &store) const; // Color Faust graphs the same way Faust does when it renders to SVG.

            void LayoutFlowGrid(TransientStore &store) const;
            void LayoutFaust(TransientStore &store) const; // Layout Faust graphs the same way Faust does when it renders to SVG.

            static const char *GetColorName(FlowGridGraphCol idx);
        );

        UIMember(
            Params,
            Prop(Bool, HeaderTitles, true);
            // In frame-height units:
            Prop(Float, MinHorizontalItemWidth, 4, 2, 8);
            Prop(Float, MaxHorizontalItemWidth, 16, 10, 24);
            Prop(Float, MinVerticalItemHeight, 4, 2, 8);
            Prop(Float, MinKnobItemSize, 3, 2, 6);

            Prop(Enum, AlignmentHorizontal, {"Left", "Middle", "Right"}, HJustify_Middle);
            Prop(Enum, AlignmentVertical, {"Top", "Middle", "Bottom"}, VJustify_Middle);
            Prop(Flags, TableFlags, TableFlagItems, TableFlags_Borders | TableFlags_Reorderable | TableFlags_Hideable);
            Prop_(
                Enum, WidthSizingPolicy,
                "?StretchFlexibleOnly: If a table contains only fixed-width items, it won't stretch to fill available width.\n"
                "StretchToFill: If a table contains only fixed-width items, allow columns to stretch to fill available width.\n"
                "Balanced: All param types are given flexible-width, weighted by their minimum width. (Looks more balanced, but less expansion room for wide items).",
                {"StretchToFill", "StretchFlexibleOnly", "Balanced"},
                ParamsWidthSizingPolicy_StretchFlexibleOnly
            )
        );

        Prop(Float, FlashDurationSec, 0.6, 0.1, 5);
        Prop(Matrix, Matrix);
        Prop(Graph, Graph);
        Prop(Params, Params);
        Prop(Colors, Colors, FlowGridCol_COUNT, GetColorName);

        void ColorsDark(TransientStore &store) const;
        void ColorsLight(TransientStore &store) const;
        void ColorsClassic(TransientStore &store) const;

        static const char *GetColorName(FlowGridCol idx);
    );

    struct ImGuiStyle : UIStateMember {
        ImGuiStyle(StateMember *parent, string_view path_segment, string_view name_help = "");

        static vector<ImVec4> ColorPresetBuffer;

        struct ImGuiColors : Colors {
            ImGuiColors(StateMember *parent, string_view path_segment, string_view name_help);
        };

        void Apply(ImGuiContext *ctx) const;
        void ColorsDark(TransientStore &) const;
        void ColorsLight(TransientStore &) const;
        void ColorsClassic(TransientStore &) const;

        static constexpr float FontAtlasScale = 2; // We rasterize to a scaled-up texture and scale down the font size globally, for sharper text.

        // See `ImGui::ImGuiStyle` for field descriptions.
        // Initial values copied from `ImGui::ImGuiStyle()` default constructor.
        // Ranges copied from `ImGui::StyleEditor`.
        // Double-check everything's up-to-date from time to time!

        // Main
        Prop(Vec2Linked, WindowPadding, {8, 8}, 0, 20, "%.0f");
        Prop(Vec2Linked, FramePadding, {4, 3}, 0, 20, false, "%.0f");
        Prop(Vec2Linked, CellPadding, {4, 2}, 0, 20, false, "%.0f");
        Prop(Vec2, ItemSpacing, {8, 4}, 0, 20, "%.0f");
        Prop(Vec2Linked, ItemInnerSpacing, {4, 4}, 0, 20, true, "%.0f");
        Prop(Vec2Linked, TouchExtraPadding, {0, 0}, 0, 10, true, "%.0f");
        Prop(Float, IndentSpacing, 21, 0, 30, "%.0f");
        Prop(Float, ScrollbarSize, 14, 1, 20, "%.0f");
        Prop(Float, GrabMinSize, 12, 1, 20, "%.0f");

        // Borders
        Prop(Float, WindowBorderSize, 1, 0, 1, "%.0f");
        Prop(Float, ChildBorderSize, 1, 0, 1, "%.0f");
        Prop(Float, FrameBorderSize, 0, 0, 1, "%.0f");
        Prop(Float, PopupBorderSize, 1, 0, 1, "%.0f");
        Prop(Float, TabBorderSize, 0, 0, 1, "%.0f");

        // Rounding
        Prop(Float, WindowRounding, 0, 0, 12, "%.0f");
        Prop(Float, ChildRounding, 0, 0, 12, "%.0f");
        Prop(Float, FrameRounding, 0, 0, 12, "%.0f");
        Prop(Float, PopupRounding, 0, 0, 12, "%.0f");
        Prop(Float, ScrollbarRounding, 9, 0, 12, "%.0f");
        Prop(Float, GrabRounding, 0, 0, 12, "%.0f");
        Prop(Float, LogSliderDeadzone, 4, 0, 12, "%.0f");
        Prop(Float, TabRounding, 4, 0, 12, "%.0f");

        // Alignment
        Prop(Vec2, WindowTitleAlign, {0, 0.5}, 0, 1, "%.2f");
        Prop(Enum, WindowMenuButtonPosition, {"Left", "Right"}, Dir_Left);
        Prop(Enum, ColorButtonPosition, {"Left", "Right"}, Dir_Right);
        Prop_(Vec2Linked, ButtonTextAlign, "?Alignment applies when a button is larger than its text content.", {0.5, 0.5}, 0, 1, "%.2f");
        Prop_(Vec2Linked, SelectableTextAlign, "?Alignment applies when a selectable is larger than its text content.", {0, 0}, 0, 1, "%.2f");

        // Safe area padding
        Prop_(Vec2Linked, DisplaySafeAreaPadding, "?Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured).", {3, 3}, 0, 30, "%.0f");

        // Rendering
        Prop_(Bool, AntiAliasedLines, "Anti-aliased lines?When disabling anti-aliasing lines, you'll probably want to disable borders in your style as well.", true);
        Prop_(Bool, AntiAliasedLinesUseTex, "Anti-aliased lines use texture?Faster lines using texture data. Require backend to render with bilinear filtering (not point/nearest filtering).", true);
        Prop_(Bool, AntiAliasedFill, "Anti-aliased fill", true);
        Prop_(Float, CurveTessellationTol, "Curve tesselation tolerance", 1.25, 0.1, 10, "%.2f", SliderFlags_None, 0.02f);
        Prop(Float, CircleTessellationMaxError, 0.3, 0.1, 5, "%.2f");
        Prop(Float, Alpha, 1, 0.2, 1, "%.2f", SliderFlags_None, 0.005); // Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets).
        Prop_(Float, DisabledAlpha, "?Additional alpha multiplier for disabled items (multiply over current value of Alpha).", 0.6, 0, 1, "%.2f", SliderFlags_None, 0.005);

        // Fonts
        Prop(Int, FontIndex);
        Prop_(Float, FontScale, "?Global font scale (low-quality!)", 1, 0.3, 2, "%.2f", SliderFlags_AlwaysClamp, 0.005f);

        // Not editable todo delete?
        Prop(Float, TabMinWidthForCloseButton, 0);
        Prop(Vec2Linked, DisplayWindowPadding, {19, 19});
        Prop(Vec2, WindowMinSize, {32, 32});
        Prop(Float, MouseCursorScale, 1);
        Prop(Float, ColumnsMinSpacing, 6);

        Prop(ImGuiColors, Colors);

    protected:
        void Render() const override;
    };

    struct ImPlotStyle : UIStateMember {
        ImPlotStyle(StateMember *parent, string_view path_segment, string_view name_help = "");

        static vector<ImVec4> ColorPresetBuffer;

        struct ImPlotColors : Colors {
            ImPlotColors(StateMember *parent, string_view path_segment, string_view name_help);
        };

        void Apply(ImPlotContext *ctx) const;
        void ColorsAuto(TransientStore &store) const;
        void ColorsDark(TransientStore &store) const;
        void ColorsLight(TransientStore &store) const;
        void ColorsClassic(TransientStore &store) const;

        // See `ImPlotStyle` for field descriptions.
        // Initial values copied from `ImPlotStyle()` default constructor.
        // Ranges copied from `ImPlot::StyleEditor`.
        // Double-check everything's up-to-date from time to time!

        // Item styling
        Prop(Float, LineWeight, 1, 0, 5, "%.1f");
        Prop(Float, MarkerSize, 4, 2, 10, "%.1f");
        Prop(Float, MarkerWeight, 1, 0, 5, "%.1f");
        Prop(Float, FillAlpha, 1, 0, 1, "%.2f");
        Prop(Float, ErrorBarSize, 5, 0, 10, "%.1f");
        Prop(Float, ErrorBarWeight, 1.5, 0, 5, "%.1f");
        Prop(Float, DigitalBitHeight, 8, 0, 20, "%.1f");
        Prop(Float, DigitalBitGap, 4, 0, 20, "%.1f");

        // Plot styling
        Prop(Float, PlotBorderSize, 1, 0, 2, "%.0f");
        Prop(Float, MinorAlpha, 0.25, 1, 0, "%.2f");
        Prop(Vec2Linked, MajorTickLen, {10, 10}, 0, 20, "%.0f");
        Prop(Vec2Linked, MinorTickLen, {5, 5}, 0, 20, "%.0f");
        Prop(Vec2Linked, MajorTickSize, {1, 1}, 0, 2, "%.1f");
        Prop(Vec2Linked, MinorTickSize, {1, 1}, 0, 2, "%.1f");
        Prop(Vec2Linked, MajorGridSize, {1, 1}, 0, 2, "%.1f");
        Prop(Vec2Linked, MinorGridSize, {1, 1}, 0, 2, "%.1f");
        Prop(Vec2, PlotDefaultSize, {400, 300}, 0, 1000, "%.0f");
        Prop(Vec2, PlotMinSize, {200, 150}, 0, 300, "%.0f");

        // Plot padding
        Prop(Vec2Linked, PlotPadding, {10, 10}, 0, 20, "%.0f");
        Prop(Vec2Linked, LabelPadding, {5, 5}, 0, 20, "%.0f");
        Prop(Vec2Linked, LegendPadding, {10, 10}, 0, 20, "%.0f");
        Prop(Vec2Linked, LegendInnerPadding, {5, 5}, 0, 10, "%.0f");
        Prop(Vec2, LegendSpacing, {5, 0}, 0, 5, "%.0f");
        Prop(Vec2Linked, MousePosPadding, {10, 10}, 0, 20, "%.0f");
        Prop(Vec2Linked, AnnotationPadding, {2, 2}, 0, 5, "%.0f");
        Prop(Vec2Linked, FitPadding, {0, 0}, 0, 0.2, "%.2f");

        Prop(ImPlotColors, Colors);
        Prop(Bool, UseLocalTime);
        Prop(Bool, UseISO8601);
        Prop(Bool, Use24HourClock);

        Prop(Int, Marker, 0); // Not editable todo delete?

    protected:
        void Render() const override;
    };

    Prop_(ImGuiStyle, ImGui, "?Configure style for base UI");
    Prop_(ImPlotStyle, ImPlot, "?Configure style for plots");
    Prop_(FlowGridStyle, FlowGrid, "?Configure application-specific style");
};
} // namespace FlowGrid

template<typename T>
struct ImChunkStream;

struct ImGuiDockNodeSettings;

template<typename T>
struct ImVector;

// These Dock/Window/Table settings are `StateMember` duplicates of those in `imgui.cpp`.
// They are stored here a structs-of-arrays (vs. arrays-of-structs)
// todo These will show up counter-intuitively in the json state viewers.
//  Use Raw/Formatted settings in state viewers to:
//  * convert structs-of-arrays to arrays-of-structs,
//  * unpack positions/sizes
Member(
    DockNodeSettings,

    void Set(const ImVector<ImGuiDockNodeSettings> &, TransientStore &) const;
    void Apply(ImGuiContext *) const;

    Prop(Vector<ID>, NodeId);
    Prop(Vector<ID>, ParentNodeId);
    Prop(Vector<ID>, ParentWindowId);
    Prop(Vector<ID>, SelectedTabId);
    Prop(Vector<int>, SplitAxis);
    Prop(Vector<int>, Depth);
    Prop(Vector<int>, Flags);
    Prop(Vector<U32>, Pos); // Packed ImVec2ih
    Prop(Vector<U32>, Size); // Packed ImVec2ih
    Prop(Vector<U32>, SizeRef); // Packed ImVec2ih
);

struct ImGuiWindowSettings;
struct ImGuiTableSettings;

Member(
    WindowSettings,

    void Set(ImChunkStream<ImGuiWindowSettings> &, TransientStore &) const;
    void Apply(ImGuiContext *) const;

    Prop(Vector<ID>, Id);
    Prop(Vector<ID>, ClassId);
    Prop(Vector<ID>, ViewportId);
    Prop(Vector<ID>, DockId);
    Prop(Vector<int>, DockOrder);
    Prop(Vector<U32>, Pos); // Packed ImVec2ih
    Prop(Vector<U32>, Size); // Packed ImVec2ih
    Prop(Vector<U32>, ViewportPos); // Packed ImVec2ih
    Prop(Vector<bool>, Collapsed);
);

Member(
    TableColumnSettings,
    // [table_index][column_index]
    Prop(Vector2D<float>, WidthOrWeight);
    Prop(Vector2D<ID>, UserID);
    Prop(Vector2D<int>, Index);
    Prop(Vector2D<int>, DisplayOrder);
    Prop(Vector2D<int>, SortOrder);
    Prop(Vector2D<int>, SortDirection);
    Prop(Vector2D<bool>, IsEnabled); // "Visible" in ini file
    Prop(Vector2D<bool>, IsStretch);
);

Member(
    TableSettings,

    void Set(ImChunkStream<ImGuiTableSettings> &, TransientStore &store) const;
    void Apply(ImGuiContext *) const;

    Prop(Vector<ImGuiID>, ID);
    Prop(Vector<int>, SaveFlags);
    Prop(Vector<float>, RefScale);
    Prop(Vector<Count>, ColumnsCount);
    Prop(Vector<Count>, ColumnsCountMax);
    Prop(Vector<bool>, WantApply);
    Prop(TableColumnSettings, Columns);
);

Member(
    ImGuiSettings,

    Store Set(ImGuiContext *ctx) const;
    // Inverse of above constructor. `imgui_context.settings = this`
    // Should behave just like `ImGui::LoadIniSettingsFromMemory`, but using the structured `...Settings` members
    //  in this struct instead of the serialized .ini text format.
    void Apply(ImGuiContext *ctx) const;

    Prop(DockNodeSettings, Nodes);
    Prop(WindowSettings, Windows);
    Prop(TableSettings, Tables);
);

WindowMember(Info);
WindowMember(StackTool);
WindowMember(DebugLog);

using ImGuiFileDialogFlags = int;
// Copied from `ImGuiFileDialog` source with a different name to avoid redefinition. Brittle but we can avoid an include this way.
constexpr ImGuiFileDialogFlags FileDialogFlags_ConfirmOverwrite = 1 << 0;
constexpr ImGuiFileDialogFlags FileDialogFlags_Modal = 1 << 9;
constexpr ImGuiFileDialogFlags FileDialogFlags_Default = FileDialogFlags_ConfirmOverwrite | FileDialogFlags_Modal;

struct FileDialogData {
    string title = "Choose file", filters, file_path = ".", default_file_name;
    bool save_mode = false;
    int max_num_selections = 1;
    ImGuiFileDialogFlags flags = FileDialogFlags_Default;
};

// `FileDialog` is a window, but it's managed by ImGuiFileDialog, so we don't use a `Window` type.
UIMember(
    FileDialog,
    void Set(const FileDialogData &data, TransientStore &) const;

    Prop(Bool, Visible);
    Prop(Bool, SaveMode); // The same file dialog instance is used for both saving & opening files.
    Prop(Int, MaxNumSelections, 1);
    Prop(Int, Flags, FileDialogFlags_Default);
    Prop(String, Title, "Choose file");
    Prop(String, Filters);
    Prop(String, FilePath, ".");
    Prop(String, DefaultFileName);
);

//-----------------------------------------------------------------------------
// [SECTION] Main application `State`
//-----------------------------------------------------------------------------
struct OpenRecentProject : MenuItemDrawable {
    void MenuItem() const override;
};

UIMember(
    State,

    OpenRecentProject open_recent_project{};
    const Menu MainMenu{
        {
            Menu("File", {OpenEmptyProject{}, ShowOpenProjectDialog{}, open_recent_project, OpenDefaultProject{}, SaveCurrentProject{}, SaveDefaultProject{}}),
            Menu("Edit", {Undo{}, Redo{}}),
            Menu(
                "Windows",
                {
                    Menu("Debug", {DebugLog, StackTool, StateViewer, StatePathUpdateFrequency, StateMemoryEditor, ProjectPreview}),
                    Menu(
                        "Faust",
                        {
                            Menu("Editor", {Audio.Faust.Editor, Audio.Faust.Editor.Metrics}),
                            Audio.Faust.Graph,
                            Audio.Faust.Params,
                            Audio.Faust.Log,
                        }
                    ),
                    Audio,
                    Metrics,
                    Style,
                    Demo,
                }
            ),
        },
        true};

    void Update(const StateAction &, TransientStore &) const;
    void Apply(UIContext::Flags) const;

    WindowMember_(
        UIProcess,
        false,
        Prop_(Bool, Running, std::format("?Disabling ends the {} process.\nEnabling will start the process up again.", Name), true);
    );

    Prop(ImGuiSettings, ImGuiSettings);
    Prop(fg::Style, Style);
    Prop(Audio, Audio);
    Prop(ApplicationSettings, ApplicationSettings);
    Prop(UIProcess, UiProcess);
    Prop(FileDialog, FileDialog);
    Prop(Info, Info);

    Prop(Demo, Demo);
    Prop(Metrics, Metrics);
    Prop(StackTool, StackTool);
    Prop(DebugLog, DebugLog);

    Prop(StateViewer, StateViewer);
    Prop(StateMemoryEditor, StateMemoryEditor);
    Prop(StatePathUpdateFrequency, StatePathUpdateFrequency);
    Prop(ProjectPreview, ProjectPreview);
);

/**
Declare global read-only accessor for the canonical state instance `s`.

`s` is a read-only structured representation of its underlying store (of type `Store`, which itself is an `immer::map<Path, Primitive>`).
It provides a complete nested struct representation of the state, along with additional metadata about each state member, such as its `Path`/`ID`/`Name`/`Info`.
Basically, it contains all data for each state member except its _actual value_ (a `Primitive`, struct of `Primitive`s, or collection of either).
(Actually, each primitive leaf value is cached on its respective `Field`, but this is a technicality - the `Store` is conceptually the source of truth.)

`s` has an immutable assignment operator, which return a modified copy of the `Store` value resulting from applying the assignment to the provided `Store`.
(Note that this is only _conceptually_ a copy - see [Application Architecture](https://github.com/khiner/flowgrid#application-architecture) for more details.)

Usage example:

```cpp
// Get a read-only reference to the complete, current, structured audio state instance:
const Audio &audio = s.Audio;
```
*/
extern const State &s;
