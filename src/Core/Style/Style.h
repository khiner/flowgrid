#pragma once

#include "Core/Container/Vec2.h"
#include "Core/Primitive/Enum.h"
#include "Core/Primitive/UInt.h"
#include "Core/UI/Styling.h"

#include "ProjectStyle.h"

struct ImGuiContext;
struct ImPlotContext;

// Copy of some of ImGui's flags, to avoid including `imgui.h` in this header.
// Be sure to keep these in sync, because they are used directly as values for their ImGui counterparts.
enum SliderFlags_ {
    SliderFlags_None = 0,
    SliderFlags_AlwaysClamp = 1 << 4, // Clamp value to min/max bounds when input manually with CTRL+Click. By default CTRL+Click allows going out of bounds.
    SliderFlags_Logarithmic = 1 << 5, // Make the widget logarithmic (linear otherwise). Consider using ImGuiSliderFlags_NoRoundToFormat with this if using a format-string with small amount of digits.
};

namespace flowgrid {
struct Style : ActionProducerComponent<ProjectStyle::ProducedActionType> {
    using ActionProducerComponent::ActionProducerComponent;

    struct ImGuiStyle : ActionProducerComponent<ProducedActionType>, ChangeListener {
        ImGuiStyle(ArgsT &&);
        ~ImGuiStyle();

        static std::vector<ImVec4> ColorsDark, ColorsLight, ColorsClassic;
        inline static bool IsChanged{false};

        struct ImGuiColors : Colors {
            ImGuiColors(ComponentArgs &&);
        };

        void OnComponentChanged() override { IsChanged = true; }
        void UpdateIfChanged(ImGuiContext *ctx) const;

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
        Prop(UInt, FontIndex);
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

    struct ImPlotStyle : ActionProducerComponent<ProducedActionType>, ChangeListener {
        ImPlotStyle(ArgsT &&);
        ~ImPlotStyle();

        inline static bool IsChanged{false};

        static std::vector<ImVec4> ColorPresetBuffer;
        static std::vector<ImVec4> ColorsAuto, ColorsDark, ColorsLight, ColorsClassic;

        struct ImPlotColors : Colors {
            ImPlotColors(ComponentArgs &&);
        };

        void OnComponentChanged() override { IsChanged = true; }
        void UpdateIfChanged(ImPlotContext *ctx) const;

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

        Prop(UInt, Marker, 0); // Not editable todo delete?

    protected:
        void Render() const override;
    };

    ChildProducerProp_(ImGuiStyle, ImGui, "?Configure base UI style");
    ChildProducerProp_(ImPlotStyle, ImPlot, "?Configure plot style");
    ChildProducerProp_(ProjectStyle, Project, "?Configure FlowGrid project style");

protected:
    void Render() const override;
};
} // namespace flowgrid
