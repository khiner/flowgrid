#include "../Context.h"
#include "Widgets.h"
#include "Menu.h"

#include "implot_internal.h"

void ShowColorEditor(const JsonPath &path, int color_count, const std::function<const char *(int)> &GetColorName) {
    if (ImGui::BeginTabItem("Colors")) {
        static ImGuiTextFilter filter;
        filter.Draw("Filter colors", ImGui::GetFontSize() * 16);

        static ImGuiColorEditFlags alpha_flags = 0;
        if (ImGui::RadioButton("Opaque", alpha_flags == ImGuiColorEditFlags_None)) { alpha_flags = ImGuiColorEditFlags_None; }
        ImGui::SameLine();
        if (ImGui::RadioButton("Alpha", alpha_flags == ImGuiColorEditFlags_AlphaPreview)) { alpha_flags = ImGuiColorEditFlags_AlphaPreview; }
        ImGui::SameLine();
        if (ImGui::RadioButton("Both", alpha_flags == ImGuiColorEditFlags_AlphaPreviewHalf)) { alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf; }
        ImGui::SameLine();
        HelpMarker(
            "In the color list:\n"
            "Left-click on color square to open color picker,\n"
            "Right-click to open edit options menu.");

        ImGui::BeginChild("##colors", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NavFlattened);
        ImGui::PushItemWidth(-160);
        for (int i = 0; i < color_count; i++) {
            const char *name = GetColorName(i);
            if (!filter.PassFilter(name)) continue;

            ImGui::PushID(i);
            fg::ColorEdit4(path / i, ImGuiColorEditFlags_AlphaBar | alpha_flags);
            ImGui::SameLine(0.0f, s.style.imgui.ItemInnerSpacing.x);
            ImGui::TextUnformatted(name);
            ImGui::PopID();
        }
        ImGui::PopItemWidth();
        ImGui::EndChild();

        ImGui::EndTabItem();
    }
}

// Returns `true` if style changes.
void Style::ImGuiStyleEditor() {
    static int style_idx = -1;
    if (ImGui::Combo("Colors##Selector", &style_idx, "Dark\0Light\0Classic\0")) q(set_imgui_color_style{style_idx});
//    ImGui::ShowFontSelector("Fonts##Selector"); // TODO

    // Simplified Settings (expose floating-pointer border sizes as boolean representing 0.0f or 1.0f)
    // TODO match with imgui
    if (fg::SliderFloat(sp(s.style.imgui.FrameRounding), 0.0f, 12.0f, "%.0f")) {
        q(set_value{sp(s.style.imgui.GrabRounding), s.style.imgui.FrameRounding}); // Make GrabRounding always the same value as FrameRounding
    }
    {
        bool border = s.style.imgui.WindowBorderSize > 0.0f;
        if (ImGui::Checkbox("WindowBorder", &border)) q(set_value{sp(s.style.imgui.WindowBorderSize), border ? 1.0f : 0.0f});
    }
    ImGui::SameLine();
    {
        bool border = s.style.imgui.FrameBorderSize > 0.0f;
        if (ImGui::Checkbox("FrameBorder", &border)) q(set_value{sp(s.style.imgui.FrameBorderSize), border ? 1.0f : 0.0f});
    }
    ImGui::SameLine();
    {
        bool border = s.style.imgui.PopupBorderSize > 0.0f;
        if (ImGui::Checkbox("PopupBorder", &border)) q(set_value{sp(s.style.imgui.PopupBorderSize), border ? 1.0f : 0.0f});
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("##ImGuiStyleEditor", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Sizes")) {
            ImGui::Text("Main");
            fg::SliderFloat2(sp(s.style.imgui.WindowPadding), 0.0f, 20.0f, "%.0f");
            fg::SliderFloat2(sp(s.style.imgui.FramePadding), 0.0f, 20.0f, "%.0f");
            fg::SliderFloat2(sp(s.style.imgui.CellPadding), 0.0f, 20.0f, "%.0f");
            fg::SliderFloat2(sp(s.style.imgui.ItemSpacing), 0.0f, 20.0f, "%.0f");
            fg::SliderFloat2(sp(s.style.imgui.ItemInnerSpacing), 0.0f, 20.0f, "%.0f");
            fg::SliderFloat2(sp(s.style.imgui.TouchExtraPadding), 0.0f, 10.0f, "%.0f");
            fg::SliderFloat(sp(s.style.imgui.IndentSpacing), 0.0f, 30.0f, "%.0f");
            fg::SliderFloat(sp(s.style.imgui.ScrollbarSize), 1.0f, 20.0f, "%.0f");
            fg::SliderFloat(sp(s.style.imgui.GrabMinSize), 1.0f, 20.0f, "%.0f");
            ImGui::Text("Borders");
            fg::SliderFloat(sp(s.style.imgui.WindowBorderSize), 0.0f, 1.0f, "%.0f");
            fg::SliderFloat(sp(s.style.imgui.ChildBorderSize), 0.0f, 1.0f, "%.0f");
            fg::SliderFloat(sp(s.style.imgui.PopupBorderSize), 0.0f, 1.0f, "%.0f");
            fg::SliderFloat(sp(s.style.imgui.FrameBorderSize), 0.0f, 1.0f, "%.0f");
            fg::SliderFloat(sp(s.style.imgui.TabBorderSize), 0.0f, 1.0f, "%.0f");
            ImGui::Text("Rounding");
            fg::SliderFloat(sp(s.style.imgui.WindowRounding), 0.0f, 12.0f, "%.0f");
            fg::SliderFloat(sp(s.style.imgui.ChildRounding), 0.0f, 12.0f, "%.0f");
            fg::SliderFloat(sp(s.style.imgui.FrameRounding), 0.0f, 12.0f, "%.0f");
            fg::SliderFloat(sp(s.style.imgui.PopupRounding), 0.0f, 12.0f, "%.0f");
            fg::SliderFloat(sp(s.style.imgui.ScrollbarRounding), 0.0f, 12.0f, "%.0f");
            fg::SliderFloat(sp(s.style.imgui.GrabRounding), 0.0f, 12.0f, "%.0f");
            fg::SliderFloat(sp(s.style.imgui.LogSliderDeadzone), 0.0f, 12.0f, "%.0f");
            fg::SliderFloat(sp(s.style.imgui.TabRounding), 0.0f, 12.0f, "%.0f");
            ImGui::Text("Alignment");
            fg::SliderFloat2(sp(s.style.imgui.WindowTitleAlign), 0.0f, 1.0f, "%.2f");
            fg::Combo(sp(s.style.imgui.WindowMenuButtonPosition), "None\0Left\0Right\0");
            fg::Combo(sp(s.style.imgui.ColorButtonPosition), "Left\0Right\0");
            fg::SliderFloat2(sp(s.style.imgui.ButtonTextAlign), 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            HelpMarker("Alignment applies when a button is larger than its text content.");
            fg::SliderFloat2(sp(s.style.imgui.SelectableTextAlign), 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            HelpMarker("Alignment applies when a selectable is larger than its text content.");
            ImGui::Text("Safe Area Padding");
            ImGui::SameLine();
            HelpMarker("Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured).");
            fg::SliderFloat2(sp(s.style.imgui.DisplaySafeAreaPadding), 0.0f, 30.0f, "%.0f");
            ImGui::EndTabItem();
        }

        ShowColorEditor(sp(s.style.imgui.Colors), ImGuiCol_COUNT, ImGui::GetStyleColorName);

//        if (ImGui::BeginTabItem("Fonts")) {
//            ImGuiIO &io = ImGui::GetIO();
//            ImFontAtlas *atlas = io.Fonts;
//            HelpMarker("Read FAQ and docs/FONTS.md for details on font loading.");
//            ImGui::ShowFontAtlas(atlas);
//
//            // Post-baking font scaling. Note that this is NOT the nice way of scaling fonts, read below.
//            // (we enforce hard clamping manually as by default DragFloat/SliderFloat allows CTRL+Click text to get out of bounds).
//            const float MIN_SCALE = 0.3f;
//            const float MAX_SCALE = 2.0f;
//            HelpMarker(
//                "Those are old settings provided for convenience.\n"
//                "However, the _correct_ way of scaling your UI is currently to reload your font at the designed size, "
//                "rebuild the font atlas, and call style.ScaleAllSizes() on a reference ImGuiStyle structure.\n"
//                "Using those settings here will give you poor quality results.");
//            static float window_scale = 1.0f;
//            ImGui::PushItemWidth(ImGui::GetFontSize() * 8);
//            if (fg::DragFloat("window scale", &window_scale, 0.005f, MIN_SCALE, MAX_SCALE, "%.2f", ImGuiSliderFlags_AlwaysClamp)) // Scale only this window
//                ImGui::SetWindowFontScale(window_scale);
//            fg::DragFloat("global scale", &io.FontGlobalScale, 0.005f, MIN_SCALE, MAX_SCALE, "%.2f", ImGuiSliderFlags_AlwaysClamp); // Scale everything
//            ImGui::PopItemWidth();
//
//            ImGui::EndTabItem();
//        }

        if (ImGui::BeginTabItem("Rendering")) {
            fg::Checkbox(sp(s.style.imgui.AntiAliasedLines), "Anti-aliased lines");
            ImGui::SameLine();
            HelpMarker("When disabling anti-aliasing lines, you'll probably want to disable borders in your style as well.");

            fg::Checkbox(sp(s.style.imgui.AntiAliasedLinesUseTex), "Anti-aliased lines use texture");
            ImGui::SameLine();
            HelpMarker("Faster lines using texture data. Require backend to render with bilinear filtering (not point/nearest filtering).");

            fg::Checkbox(sp(s.style.imgui.AntiAliasedFill), "Anti-aliased fill");
            ImGui::PushItemWidth(ImGui::GetFontSize() * 8);
            fg::DragFloat(sp(s.style.imgui.CurveTessellationTol), 0.02f, 0.10f, 10.0f, "%.2f", ImGuiSliderFlags_None, "Curve Tessellation Tolerance");

            // When editing the "Circle Segment Max Error" value, draw a preview of its effect on auto-tessellated circles.
            fg::DragFloat(sp(s.style.imgui.CircleTessellationMaxError), 0.005f, 0.10f, 5.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::IsItemActive()) {
                ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
                ImGui::BeginTooltip();
                ImGui::TextUnformatted("(R = radius, N = number of segments)");
                ImGui::Spacing();
                ImDrawList *draw_list = ImGui::GetWindowDrawList();
                const float min_widget_width = ImGui::CalcTextSize("N: MMM\nR: MMM").x;
                for (int n = 0; n < 8; n++) {
                    const float RAD_MIN = 5.0f;
                    const float RAD_MAX = 70.0f;
                    const float rad = RAD_MIN + (RAD_MAX - RAD_MIN) * (float) n / (8.0f - 1.0f);

                    ImGui::BeginGroup();

                    ImGui::Text("R: %.f\nN: %d", rad, draw_list->_CalcCircleAutoSegmentCount(rad));

                    const float canvas_width = ImMax(min_widget_width, rad * 2.0f);
                    const float offset_x = floorf(canvas_width * 0.5f);
                    const float offset_y = floorf(RAD_MAX);

                    const ImVec2 p1 = ImGui::GetCursorScreenPos();
                    draw_list->AddCircle(ImVec2(p1.x + offset_x, p1.y + offset_y), rad, ImGui::GetColorU32(ImGuiCol_Text));
                    ImGui::Dummy(ImVec2(canvas_width, RAD_MAX * 2));

                    ImGui::EndGroup();
                    ImGui::SameLine();
                }
                ImGui::EndTooltip();
            }
            ImGui::SameLine();
            HelpMarker("When drawing circle primitives with \"num_segments == 0\" tesselation will be calculated automatically.");

            // Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets). But application code could have a toggle to switch between zero and non-zero.
            fg::DragFloat(sp(s.style.imgui.Alpha), 0.005f, 0.20f, 1.0f, "%.2f");
            fg::DragFloat(sp(s.style.imgui.DisabledAlpha), 0.005f, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            HelpMarker("Additional alpha multiplier for disabled items (multiply over current value of Alpha).");
            ImGui::PopItemWidth();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void Style::ImPlotStyleEditor() {
    static int style_idx = -1;
    if (ImGui::Combo("Colors##Selector", &style_idx, "Auto\0Classic\0Dark\0Light\0")) q(set_implot_color_style{style_idx});

    if (ImGui::BeginTabBar("##ImPlotStyleEditor")) {
        if (ImGui::BeginTabItem("Variables")) {
            ImGui::Text("Item Styling");
            fg::SliderFloat(sp(s.style.implot.LineWeight), 0.0f, 5.0f, "%.1f");
            fg::SliderFloat(sp(s.style.implot.MarkerSize), 2.0f, 10.0f, "%.1f");
            fg::SliderFloat(sp(s.style.implot.MarkerWeight), 0.0f, 5.0f, "%.1f");
            fg::SliderFloat(sp(s.style.implot.FillAlpha), 0.0f, 1.0f, "%.2f");
            fg::SliderFloat(sp(s.style.implot.ErrorBarSize), 0.0f, 10.0f, "%.1f");
            fg::SliderFloat(sp(s.style.implot.ErrorBarWeight), 0.0f, 5.0f, "%.1f");
            fg::SliderFloat(sp(s.style.implot.DigitalBitHeight), 0.0f, 20.0f, "%.1f");
            fg::SliderFloat(sp(s.style.implot.DigitalBitGap), 0.0f, 20.0f, "%.1f");

            ImGui::Text("Plot Styling");
            fg::SliderFloat(sp(s.style.implot.PlotBorderSize), 0.0f, 2.0f, "%.0f");
            fg::SliderFloat(sp(s.style.implot.MinorAlpha), 0.0f, 1.0f, "%.2f");
            fg::SliderFloat2(sp(s.style.implot.MajorTickLen), 0.0f, 20.0f, "%.0f");
            fg::SliderFloat2(sp(s.style.implot.MinorTickLen), 0.0f, 20.0f, "%.0f");
            fg::SliderFloat2(sp(s.style.implot.MajorTickSize), 0.0f, 2.0f, "%.1f");
            fg::SliderFloat2(sp(s.style.implot.MinorTickSize), 0.0f, 2.0f, "%.1f");
            fg::SliderFloat2(sp(s.style.implot.MajorGridSize), 0.0f, 2.0f, "%.1f");
            fg::SliderFloat2(sp(s.style.implot.MinorGridSize), 0.0f, 2.0f, "%.1f");
            fg::SliderFloat2(sp(s.style.implot.PlotDefaultSize), 0.0f, 1000, "%.0f");
            fg::SliderFloat2(sp(s.style.implot.PlotMinSize), 0.0f, 300, "%.0f");

            ImGui::Text("Plot Padding");
            fg::SliderFloat2(sp(s.style.implot.PlotPadding), 0.0f, 20.0f, "%.0f");
            fg::SliderFloat2(sp(s.style.implot.LabelPadding), 0.0f, 20.0f, "%.0f");
            fg::SliderFloat2(sp(s.style.implot.LegendPadding), 0.0f, 20.0f, "%.0f");
            fg::SliderFloat2(sp(s.style.implot.LegendInnerPadding), 0.0f, 10.0f, "%.0f");
            fg::SliderFloat2(sp(s.style.implot.LegendSpacing), 0.0f, 5.0f, "%.0f");
            fg::SliderFloat2(sp(s.style.implot.MousePosPadding), 0.0f, 20.0f, "%.0f");
            fg::SliderFloat2(sp(s.style.implot.AnnotationPadding), 0.0f, 5.0f, "%.0f");
            fg::SliderFloat2(sp(s.style.implot.FitPadding), 0, 0.2f, "%.2f");

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Colors")) {
            static ImGuiTextFilter filter;
            filter.Draw("Filter colors", ImGui::GetFontSize() * 16);

            static ImGuiColorEditFlags alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf;
            if (ImGui::RadioButton("Opaque", alpha_flags == ImGuiColorEditFlags_None)) { alpha_flags = ImGuiColorEditFlags_None; }
            ImGui::SameLine();
            if (ImGui::RadioButton("Alpha", alpha_flags == ImGuiColorEditFlags_AlphaPreview)) { alpha_flags = ImGuiColorEditFlags_AlphaPreview; }
            ImGui::SameLine();
            if (ImGui::RadioButton("Both", alpha_flags == ImGuiColorEditFlags_AlphaPreviewHalf)) { alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf; }
            ImGui::SameLine();
            HelpMarker(
                "In the color list:\n"
                "Left-click on colored square to open color picker,\n"
                "Right-click to open edit options menu.");

            ImGui::Separator();
            ImGui::PushItemWidth(-160);
            const auto colors_path = sp(s.style.implot.Colors);
            for (int i = 0; i < ImPlotCol_COUNT; i++) {
                const char *name = ImPlot::GetStyleColorName(i);
                if (!filter.PassFilter(name)) continue;

                ImGui::PushID(i);
                ImVec4 temp = ImPlot::GetStyleColorVec4(i);
                const bool is_auto = ImPlot::IsColorAuto(i);
                if (!is_auto) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.25f);
                if (ImGui::Button("Auto")) q(set_value{colors_path / i, is_auto ? temp : IMPLOT_AUTO_COL});
                if (!is_auto) ImGui::PopStyleVar();
                ImGui::SameLine();
                fg::ColorEdit4(colors_path / i, ImGuiColorEditFlags_NoInputs | alpha_flags, name);
                ImGui::PopID();
            }
            ImGui::PopItemWidth();
            ImGui::Separator();
            ImGui::Text("Colors that are set to Auto (i.e. IMPLOT_AUTO_COL) will\n"
                        "be automatically deduced from your ImGui style or the\n"
                        "current ImPlot Colormap. If you want to style individual\n"
                        "plot items, use Push/PopStyleColor around its function.");
            ImGui::EndTabItem();
        }
        // TODO re-implement colormaps statefully
        ImGui::EndTabBar();
    }
}

void Style::FlowGridStyleEditor() {
    fg::SliderFloat(sp(s.style.flowgrid.FlashDurationSec), FlashDurationSecMin, FlashDurationSecMax, "%.3f s");
    static int style_idx = -1;
    if (ImGui::Combo("Colors##Selector", &style_idx, "Dark\0Light\0Classic\0")) q(set_flowgrid_color_style{style_idx});

    if (ImGui::BeginTabBar("##FlowGridStyleEditor")) {
        ShowColorEditor(sp(s.style.flowgrid.Colors), FlowGridCol_COUNT, FlowGridStyle::GetColorName);
        ImGui::EndTabBar();
    }
}

void Style::draw() const {
    if (ImGui::BeginTabBar("##style")) {
        if (ImGui::BeginTabItem("FlowGrid")) {
            FlowGridStyleEditor();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ImGui")) {
            ImGuiStyleEditor();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ImPlot")) {
            ImPlotStyleEditor();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
