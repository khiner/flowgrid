#include "Style.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "implot_internal.h"

using namespace ImGui;

namespace FlowGrid {
std::vector<ImVec4> Style::ImGuiStyle::ColorsDark(ImGuiCol_COUNT);
std::vector<ImVec4> Style::ImGuiStyle::ColorsLight(ImGuiCol_COUNT);
std::vector<ImVec4> Style::ImGuiStyle::ColorsClassic(ImGuiCol_COUNT);

std::vector<ImVec4> Style::ImPlotStyle::ColorsAuto(ImPlotCol_COUNT);
std::vector<ImVec4> Style::ImPlotStyle::ColorsLight(ImPlotCol_COUNT);
std::vector<ImVec4> Style::ImPlotStyle::ColorsDark(ImPlotCol_COUNT);
std::vector<ImVec4> Style::ImPlotStyle::ColorsClassic(ImPlotCol_COUNT);

Style::ImGuiStyle::ImGuiStyle(ArgsT &&args) : ActionProducerComponent(std::move(args)) {
    for (const auto *child : Children) child->RegisterChangeListener(this);

    ImGui::StyleColorsDark(ColorsDark.data());
    ImGui::StyleColorsLight(ColorsLight.data());
    ImGui::StyleColorsClassic(ColorsClassic.data());
    Colors.Set(ColorsDark);
}
Style::ImGuiStyle::~ImGuiStyle() {
    UnregisterChangeListener(this);
}

Style::ImPlotStyle::ImPlotStyle(ArgsT &&args) : ActionProducerComponent(std::move(args)) {
    for (const auto *child : Children) child->RegisterChangeListener(this);

    ImPlot::StyleColorsAuto(ColorsAuto.data());
    ImPlot::StyleColorsDark(ColorsDark.data());
    ImPlot::StyleColorsLight(ColorsLight.data());
    ImPlot::StyleColorsClassic(ColorsClassic.data());
    Colors.Set(ColorsAuto);
}
Style::ImPlotStyle::~ImPlotStyle() {
    UnregisterChangeListener(this);
}

void Style::ImGuiStyle::UpdateIfChanged(ImGuiContext *ctx) const {
    if (!IsChanged) return;

    IsChanged = false;

    auto &style = ctx->Style;
    style.Alpha = Alpha;
    style.DisabledAlpha = DisabledAlpha;
    style.WindowPadding = WindowPadding;
    style.WindowRounding = WindowRounding;
    style.WindowBorderSize = WindowBorderSize;
    style.WindowMinSize = WindowMinSize;
    style.WindowTitleAlign = WindowTitleAlign;
    style.WindowMenuButtonPosition = ImGuiDir(int(WindowMenuButtonPosition));
    style.ChildRounding = ChildRounding;
    style.ChildBorderSize = ChildBorderSize;
    style.PopupRounding = PopupRounding;
    style.PopupBorderSize = PopupBorderSize;
    style.FramePadding = FramePadding;
    style.FrameRounding = FrameRounding;
    style.FrameBorderSize = FrameBorderSize;
    style.ItemSpacing = ItemSpacing;
    style.ItemInnerSpacing = ItemInnerSpacing;
    style.CellPadding = CellPadding;
    style.TouchExtraPadding = TouchExtraPadding;
    style.IndentSpacing = IndentSpacing;
    style.ColumnsMinSpacing = ColumnsMinSpacing;
    style.ScrollbarSize = ScrollbarSize;
    style.ScrollbarRounding = ScrollbarRounding;
    style.GrabMinSize = GrabMinSize;
    style.GrabRounding = GrabRounding;
    style.LogSliderDeadzone = LogSliderDeadzone;
    style.TabRounding = TabRounding;
    style.TabBorderSize = TabBorderSize;
    style.TabMinWidthForCloseButton = TabMinWidthForCloseButton;
    style.ColorButtonPosition = ImGuiDir(int(ColorButtonPosition));
    style.ButtonTextAlign = ButtonTextAlign;
    style.SelectableTextAlign = SelectableTextAlign;
    style.DisplayWindowPadding = DisplayWindowPadding;
    style.DisplaySafeAreaPadding = DisplaySafeAreaPadding;
    style.MouseCursorScale = MouseCursorScale;
    style.AntiAliasedLines = AntiAliasedLines;
    style.AntiAliasedLinesUseTex = AntiAliasedLinesUseTex;
    style.AntiAliasedFill = AntiAliasedFill;
    style.CurveTessellationTol = CurveTessellationTol;
    style.CircleTessellationMaxError = CircleTessellationMaxError;
    for (int i = 0; i < ImGuiCol_COUNT; i++) style.Colors[i] = ColorConvertU32ToFloat4(Colors[i]);
}

void Style::ImPlotStyle::UpdateIfChanged(ImPlotContext *ctx) const {
    if (!IsChanged) return;

    IsChanged = false;

    auto &style = ctx->Style;
    style.LineWeight = LineWeight;
    style.Marker = Marker;
    style.MarkerSize = MarkerSize;
    style.MarkerWeight = MarkerWeight;
    style.FillAlpha = FillAlpha;
    style.ErrorBarSize = ErrorBarSize;
    style.ErrorBarWeight = ErrorBarWeight;
    style.DigitalBitHeight = DigitalBitHeight;
    style.DigitalBitGap = DigitalBitGap;
    style.PlotBorderSize = PlotBorderSize;
    style.MinorAlpha = MinorAlpha;
    style.MajorTickLen = MajorTickLen;
    style.MinorTickLen = MinorTickLen;
    style.MajorTickSize = MajorTickSize;
    style.MinorTickSize = MinorTickSize;
    style.MajorGridSize = MajorGridSize;
    style.MinorGridSize = MinorGridSize;
    style.PlotPadding = PlotPadding;
    style.LabelPadding = LabelPadding;
    style.LegendPadding = LegendPadding;
    style.LegendInnerPadding = LegendInnerPadding;
    style.LegendSpacing = LegendSpacing;
    style.MousePosPadding = MousePosPadding;
    style.AnnotationPadding = AnnotationPadding;
    style.FitPadding = FitPadding;
    style.PlotDefaultSize = PlotDefaultSize;
    style.PlotMinSize = PlotMinSize;
    style.Colormap = ImPlotColormap_Deep; // todo configurable
    style.UseLocalTime = UseLocalTime;
    style.UseISO8601 = UseISO8601;
    style.Use24HourClock = Use24HourClock;
    for (int i = 0; i < ImPlotCol_COUNT; i++) style.Colors[i] = Colors::U32ToFloat4(Colors[i]);
    ImPlot::BustItemCache();
}

Style::ImGuiStyle::ImGuiColors::ImGuiColors(ArgsT &&args)
    : Colors(std::move(args), ImGuiCol_COUNT, ImGui::GetStyleColorName, false) {}

Style::ImPlotStyle::ImPlotColors::ImPlotColors(ArgsT &&args)
    : Colors(std::move(args), ImPlotCol_COUNT, ImPlot::GetStyleColorName, true) {}

void Style::ImGuiStyle::Render() const {
    static int style_idx = -1;
    if (Combo("Colors##Selector", &style_idx, "Dark\0Light\0Classic\0")) Q(Action::Style::SetImGuiColorPreset{style_idx});

    const auto &io = GetIO();
    if (const auto *font_current = GetFont(); BeginCombo("Fonts", font_current->GetDebugName())) {
        for (int n = 0; n < io.Fonts->Fonts.Size; n++) {
            const auto *font = io.Fonts->Fonts[n];
            PushID(font);
            if (Selectable(font->GetDebugName(), font == font_current)) FontIndex.IssueSet(n);
            PopID();
        }
        EndCombo();
    }

    // Simplified Settings (expose floating-pointer border sizes as boolean representing 0 or 1)
    if (bool border = WindowBorderSize > 0; Checkbox("WindowBorder", &border)) WindowBorderSize.IssueSet(border ? 1.f : 0.f);
    SameLine();
    if (bool border = FrameBorderSize > 0; Checkbox("FrameBorder", &border)) FrameBorderSize.IssueSet(border ? 1.f : 0.f);
    SameLine();
    if (bool border = PopupBorderSize > 0; Checkbox("PopupBorder", &border)) PopupBorderSize.IssueSet(border ? 1.f : 0.f);

    Separator();

    if (BeginTabBar("", ImGuiTabBarFlags_None)) {
        if (BeginTabItem("Variables", nullptr, ImGuiTabItemFlags_NoPushId)) {
            Text("Main");
            WindowPadding.Draw();
            FramePadding.Draw();
            CellPadding.Draw();
            ItemSpacing.Draw();
            ItemInnerSpacing.Draw();
            TouchExtraPadding.Draw();
            IndentSpacing.Draw();
            ScrollbarSize.Draw();
            GrabMinSize.Draw();

            Text("Borders");
            WindowBorderSize.Draw();
            ChildBorderSize.Draw();
            PopupBorderSize.Draw();
            FrameBorderSize.Draw();
            TabBorderSize.Draw();

            Text("Rounding");
            WindowRounding.Draw();
            ChildRounding.Draw();
            FrameRounding.Draw();
            PopupRounding.Draw();
            ScrollbarRounding.Draw();
            GrabRounding.Draw();
            LogSliderDeadzone.Draw();
            TabRounding.Draw();

            Text("Alignment");
            WindowTitleAlign.Draw();
            WindowMenuButtonPosition.Draw();
            ColorButtonPosition.Draw();
            ButtonTextAlign.Draw();
            SelectableTextAlign.Draw();

            Text("Safe Area Padding");
            DisplaySafeAreaPadding.Draw();

            EndTabItem();
        }
        if (BeginTabItem(Colors.ImGuiLabel.c_str(), nullptr, ImGuiTabItemFlags_NoPushId)) {
            Colors.Draw();
            EndTabItem();
        }
        if (BeginTabItem("Fonts")) {
            ShowFontAtlas(io.Fonts);

            PushItemWidth(GetFontSize() * 8);
            FontScale.Draw();
            PopItemWidth();

            EndTabItem();
        }
        if (BeginTabItem("Rendering", nullptr, ImGuiTabItemFlags_NoPushId)) {
            AntiAliasedLines.Draw();
            AntiAliasedLinesUseTex.Draw();
            AntiAliasedFill.Draw();
            PushItemWidth(GetFontSize() * 8);
            CurveTessellationTol.Draw();

            // When editing the "Circle Segment Max Error" value, draw a preview of its effect on auto-tessellated circles.
            CircleTessellationMaxError.Draw();
            if (IsItemActive()) {
                SetNextWindowPos(GetCursorScreenPos());
                BeginTooltip();
                TextUnformatted("(R = radius, N = number of segments)");
                Spacing();
                ImDrawList *draw_list = GetWindowDrawList();
                const float min_widget_width = CalcTextSize("N: MMM\nR: MMM").x;
                for (int n = 0; n < 8; n++) {
                    const float RAD_MIN = 5, RAD_MAX = 70;
                    const float rad = RAD_MIN + (RAD_MAX - RAD_MIN) * float(n) / 7.0f;

                    BeginGroup();

                    Text("R: %.f\nN: %d", rad, draw_list->_CalcCircleAutoSegmentCount(rad));

                    const float canvas_width = std::max(min_widget_width, rad * 2);
                    draw_list->AddCircle(GetCursorScreenPos() + ImVec2{floorf(canvas_width * 0.5f), floorf(RAD_MAX)}, rad, GetColorU32(ImGuiCol_Text));
                    Dummy({canvas_width, RAD_MAX * 2});

                    EndGroup();
                    SameLine();
                }
                EndTooltip();
            }
            SameLine();
            HelpMarker("When drawing circle primitives with \"num_segments == 0\" tesselation will be calculated automatically.");

            Alpha.Draw();
            DisabledAlpha.Draw();
            PopItemWidth();

            EndTabItem();
        }

        EndTabBar();
    }
}

void Style::ImPlotStyle::Render() const {
    static int style_idx = -1;
    if (Combo("Colors##Selector", &style_idx, "Auto\0Dark\0Light\0Classic\0")) Q(Action::Style::SetImPlotColorPreset{style_idx});

    if (BeginTabBar("")) {
        if (BeginTabItem("Variables", nullptr, ImGuiTabItemFlags_NoPushId)) {
            Text("Item Styling");
            LineWeight.Draw();
            MarkerSize.Draw();
            MarkerWeight.Draw();
            FillAlpha.Draw();
            ErrorBarSize.Draw();
            ErrorBarWeight.Draw();
            DigitalBitHeight.Draw();
            DigitalBitGap.Draw();

            Text("Plot Styling");
            PlotBorderSize.Draw();
            MinorAlpha.Draw();
            MajorTickLen.Draw();
            MinorTickLen.Draw();
            MajorTickSize.Draw();
            MinorTickSize.Draw();
            MajorGridSize.Draw();
            MinorGridSize.Draw();
            PlotDefaultSize.Draw();
            PlotMinSize.Draw();

            Text("Plot Padding");
            PlotPadding.Draw();
            LabelPadding.Draw();
            LegendPadding.Draw();
            LegendInnerPadding.Draw();
            LegendSpacing.Draw();
            MousePosPadding.Draw();
            AnnotationPadding.Draw();
            FitPadding.Draw();

            EndTabItem();
        }
        if (BeginTabItem(Colors.ImGuiLabel.c_str(), nullptr, ImGuiTabItemFlags_NoPushId)) {
            Colors.Draw();
            EndTabItem();
        }
        EndTabBar();
    }
}

void Style::Render() const {
    RenderTabs();
}
} // namespace FlowGrid

FlowGridStyle::FlowGridStyle(ArgsT &&args) : ActionProducerComponent(std::move(args)) {
    Colors.Set(ColorsDark);
}

const char *FlowGridStyle::GetColorName(FlowGridCol idx) {
    switch (idx) {
        case FlowGridCol_GestureIndicator: return "GestureIndicator";
        case FlowGridCol_HighlightText: return "HighlightText";
        case FlowGridCol_Flash: return "Flash";
        default: return "Unknown";
    }
}

std::unordered_map<size_t, ImVec4> FlowGridStyle::ColorsDark = {
    {FlowGridCol_HighlightText, {1, 0.6, 0, 1}},
    {FlowGridCol_GestureIndicator, {0.87, 0.52, 0.32, 1}},
    {FlowGridCol_Flash, {0.26, 0.59, 0.98, 0.67}},
};
std::unordered_map<size_t, ImVec4> FlowGridStyle::ColorsLight = {
    {FlowGridCol_HighlightText, {1, 0.45, 0, 1}},
    {FlowGridCol_GestureIndicator, {0.87, 0.52, 0.32, 1}},
    {FlowGridCol_Flash, {0.26, 0.59, 0.98, 0.4}},
};
std::unordered_map<size_t, ImVec4> FlowGridStyle::ColorsClassic = {
    {FlowGridCol_HighlightText, {1, 0.6, 0, 1}},
    {FlowGridCol_GestureIndicator, {0.87, 0.52, 0.32, 1}},
    {FlowGridCol_Flash, {0.47, 0.47, 0.69, 0.4}},
};

void FlowGridStyle::Render() const {
    static int colors_idx = -1;
    if (Combo("Colors", &colors_idx, "Dark\0Light\0Classic\0")) Q(Action::Style::SetFlowGridColorPreset{colors_idx});
    FlashDurationSec.Draw();

    if (BeginTabBar("")) {
        if (BeginTabItem(Colors.ImGuiLabel.c_str(), nullptr, ImGuiTabItemFlags_NoPushId)) {
            Colors.Draw();
            EndTabItem();
        }
        EndTabBar();
    }
}
