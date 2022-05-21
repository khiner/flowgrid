#include "../context.h"
#include "../stateful_imgui.h"
#include "../imgui_helpers.h"

#include "implot_internal.h"

// From `imgui_demo.cpp`
bool ShowStyleSelector(const char *label, ImGuiStyle *dst) {
    static int style_idx = -1;
    if (ImGui::Combo(label, &style_idx, "Dark\0Light\0Classic\0")) {
        switch (style_idx) {
            case 0: ImGui::StyleColorsDark(dst);
                break;
            case 1: ImGui::StyleColorsLight(dst);
                break;
            case 2: ImGui::StyleColorsClassic(dst);
                break;
            default:break;
        }
        return true;
    }
    return false;
}

// Returns `true` if style changes.
bool Windows::StyleEditor::ImGuiStyleEditor() {
    bool changed = false;
    auto &style = ui_s.style.imgui;

    changed |= ShowStyleSelector("Colors##Selector", &style);
//    ImGui::ShowFontSelector("Fonts##Selector"); // TODO

    // Simplified Settings (expose floating-pointer border sizes as boolean representing 0.0f or 1.0f)
    if (StatefulImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f, "%.0f")) {
        style.GrabRounding = style.FrameRounding; // Make GrabRounding always the same value as FrameRounding
        changed = true;
    }
    {
        bool border = (style.WindowBorderSize > 0.0f);
        if (ImGui::Checkbox("WindowBorder", &border)) {
            style.WindowBorderSize = border ? 1.0f : 0.0f;
            changed = true;
        }
    }
    ImGui::SameLine();
    {
        bool border = (style.FrameBorderSize > 0.0f);
        if (ImGui::Checkbox("FrameBorder", &border)) {
            style.FrameBorderSize = border ? 1.0f : 0.0f;
            changed = true;
        }
    }
    ImGui::SameLine();
    {
        bool border = (style.PopupBorderSize > 0.0f);
        if (ImGui::Checkbox("PopupBorder", &border)) {
            style.PopupBorderSize = border ? 1.0f : 0.0f;
            changed = true;
        }
    }

    ImGui::Separator();

    if (ImGui::BeginTabBar("##ImGuiStyleEditor", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Sizes")) {
            ImGui::Text("Main");
            changed |= StatefulImGui::SliderFloat2("WindowPadding", (float *) &style.WindowPadding, 0.0f, 20.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat2("FramePadding", (float *) &style.FramePadding, 0.0f, 20.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat2("CellPadding", (float *) &style.CellPadding, 0.0f, 20.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat2("ItemSpacing", (float *) &style.ItemSpacing, 0.0f, 20.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat2("ItemInnerSpacing", (float *) &style.ItemInnerSpacing, 0.0f, 20.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat2("TouchExtraPadding", (float *) &style.TouchExtraPadding, 0.0f, 10.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("IndentSpacing", &style.IndentSpacing, 0.0f, 30.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("ScrollbarSize", &style.ScrollbarSize, 1.0f, 20.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("GrabMinSize", &style.GrabMinSize, 1.0f, 20.0f, "%.0f");
            ImGui::Text("Borders");
            changed |= StatefulImGui::SliderFloat("WindowBorderSize", &style.WindowBorderSize, 0.0f, 1.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("ChildBorderSize", &style.ChildBorderSize, 0.0f, 1.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("PopupBorderSize", &style.PopupBorderSize, 0.0f, 1.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("FrameBorderSize", &style.FrameBorderSize, 0.0f, 1.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("TabBorderSize", &style.TabBorderSize, 0.0f, 1.0f, "%.0f");
            ImGui::Text("Rounding");
            changed |= StatefulImGui::SliderFloat("WindowRounding", &style.WindowRounding, 0.0f, 12.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("ChildRounding", &style.ChildRounding, 0.0f, 12.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("PopupRounding", &style.PopupRounding, 0.0f, 12.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("ScrollbarRounding", &style.ScrollbarRounding, 0.0f, 12.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("GrabRounding", &style.GrabRounding, 0.0f, 12.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("LogSliderDeadzone", &style.LogSliderDeadzone, 0.0f, 12.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("TabRounding", &style.TabRounding, 0.0f, 12.0f, "%.0f");
            ImGui::Text("Alignment");
            changed |= StatefulImGui::SliderFloat2("WindowTitleAlign", (float *) &style.WindowTitleAlign, 0.0f, 1.0f, "%.2f");
            int window_menu_button_position = style.WindowMenuButtonPosition + 1;
            if (ImGui::Combo("WindowMenuButtonPosition", (int *) &window_menu_button_position, "None\0Left\0Right\0")) {
                style.WindowMenuButtonPosition = window_menu_button_position - 1;
                changed = true;
            }
            changed |= ImGui::Combo("ColorButtonPosition", (int *) &style.ColorButtonPosition, "Left\0Right\0");
            changed |= StatefulImGui::SliderFloat2("ButtonTextAlign", (float *) &style.ButtonTextAlign, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            HelpMarker("Alignment applies when a button is larger than its text content.");
            changed |= StatefulImGui::SliderFloat2("SelectableTextAlign", (float *) &style.SelectableTextAlign, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            HelpMarker("Alignment applies when a selectable is larger than its text content.");
            ImGui::Text("Safe Area Padding");
            ImGui::SameLine();
            HelpMarker("Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured).");
            changed |= StatefulImGui::SliderFloat2("DisplaySafeAreaPadding", (float *) &style.DisplaySafeAreaPadding, 0.0f, 30.0f, "%.0f");
            ImGui::EndTabItem();
        }

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
            for (int i = 0; i < ImGuiCol_COUNT; i++) {
                const char *name = ImGui::GetStyleColorName(i);
                if (!filter.PassFilter(name)) continue;

                ImGui::PushID(i);
                changed |= StatefulImGui::ColorEdit4("##color", (float *) &style.Colors[i], ImGuiColorEditFlags_AlphaBar | alpha_flags);
                ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
                ImGui::TextUnformatted(name);
                ImGui::PopID();
            }
            ImGui::PopItemWidth();
            ImGui::EndChild();

            ImGui::EndTabItem();
        }

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
//            if (StatefulImGui::DragFloat("window scale", &window_scale, 0.005f, MIN_SCALE, MAX_SCALE, "%.2f", ImGuiSliderFlags_AlwaysClamp)) // Scale only this window
//                ImGui::SetWindowFontScale(window_scale);
//            StatefulImGui::DragFloat("global scale", &io.FontGlobalScale, 0.005f, MIN_SCALE, MAX_SCALE, "%.2f", ImGuiSliderFlags_AlwaysClamp); // Scale everything
//            ImGui::PopItemWidth();
//
//            ImGui::EndTabItem();
//        }

        if (ImGui::BeginTabItem("Rendering")) {
            changed |= ImGui::Checkbox("Anti-aliased lines", &style.AntiAliasedLines);
            ImGui::SameLine();
            HelpMarker("When disabling anti-aliasing lines, you'll probably want to disable borders in your style as well.");

            changed |= ImGui::Checkbox("Anti-aliased lines use texture", &style.AntiAliasedLinesUseTex);
            ImGui::SameLine();
            HelpMarker("Faster lines using texture data. Require backend to render with bilinear filtering (not point/nearest filtering).");

            changed |= ImGui::Checkbox("Anti-aliased fill", &style.AntiAliasedFill);
            ImGui::PushItemWidth(ImGui::GetFontSize() * 8);
            changed |= StatefulImGui::DragFloat("Curve Tessellation Tolerance", &style.CurveTessellationTol, 0.02f, 0.10f, 10.0f, "%.2f");
            if (style.CurveTessellationTol < 0.10f) style.CurveTessellationTol = 0.10f;

            // When editing the "Circle Segment Max Error" value, draw a preview of its effect on auto-tessellated circles.
            changed |= StatefulImGui::DragFloat("Circle Tessellation Max Error", &style.CircleTessellationMaxError, 0.005f, 0.10f, 5.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
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
            changed |= StatefulImGui::DragFloat("Global Alpha", &style.Alpha, 0.005f, 0.20f, 1.0f, "%.2f");
            changed |= StatefulImGui::DragFloat("Disabled Alpha", &style.DisabledAlpha, 0.005f, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            HelpMarker("Additional alpha multiplier for disabled items (multiply over current value of Alpha).");
            ImGui::PopItemWidth();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    return changed;
}

// From `implot_demo.cpp`
bool ShowImPlotStyleSelector(const char *label, ImPlotStyle *dst) {
    static int style_idx = -1;
    if (ImGui::Combo(label, &style_idx, "Auto\0Classic\0Dark\0Light\0")) {
        switch (style_idx) {
            case 0: ImPlot::StyleColorsAuto(dst);
                break;
            case 1: ImPlot::StyleColorsClassic(dst);
                break;
            case 2: ImPlot::StyleColorsDark(dst);
                break;
            case 3: ImPlot::StyleColorsLight(dst);
                break;
            default:break;
        }
        return true;
    }
    return false;
}

bool Windows::StyleEditor::ImPlotStyleEditor() {
    bool changed = false;
    auto &style = ui_s.style.implot;

    changed |= ShowImPlotStyleSelector("Colors##Selector", &style);

    if (ImGui::BeginTabBar("##ImPlotStyleEditor")) {
        if (ImGui::BeginTabItem("Variables")) {
            ImGui::Text("Item Styling");
            changed |= StatefulImGui::SliderFloat("LineWeight", &style.LineWeight, 0.0f, 5.0f, "%.1f");
            changed |= StatefulImGui::SliderFloat("MarkerSize", &style.MarkerSize, 2.0f, 10.0f, "%.1f");
            changed |= StatefulImGui::SliderFloat("MarkerWeight", &style.MarkerWeight, 0.0f, 5.0f, "%.1f");
            changed |= StatefulImGui::SliderFloat("FillAlpha", &style.FillAlpha, 0.0f, 1.0f, "%.2f");
            changed |= StatefulImGui::SliderFloat("ErrorBarSize", &style.ErrorBarSize, 0.0f, 10.0f, "%.1f");
            changed |= StatefulImGui::SliderFloat("ErrorBarWeight", &style.ErrorBarWeight, 0.0f, 5.0f, "%.1f");
            changed |= StatefulImGui::SliderFloat("DigitalBitHeight", &style.DigitalBitHeight, 0.0f, 20.0f, "%.1f");
            changed |= StatefulImGui::SliderFloat("DigitalBitGap", &style.DigitalBitGap, 0.0f, 20.0f, "%.1f");

            float indent = ImGui::CalcItemWidth() - ImGui::GetFrameHeight();
            ImGui::Indent(ImGui::CalcItemWidth() - ImGui::GetFrameHeight());
            changed |= ImGui::Checkbox("AntiAliasedLines", &style.AntiAliasedLines);
            ImGui::Unindent(indent);

            ImGui::Text("Plot Styling");
            changed |= StatefulImGui::SliderFloat("PlotBorderSize", &style.PlotBorderSize, 0.0f, 2.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat("MinorAlpha", &style.MinorAlpha, 0.0f, 1.0f, "%.2f");
            changed |= StatefulImGui::SliderFloat2("MajorTickLen", (float *) &style.MajorTickLen, 0.0f, 20.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat2("MinorTickLen", (float *) &style.MinorTickLen, 0.0f, 20.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat2("MajorTickSize", (float *) &style.MajorTickSize, 0.0f, 2.0f, "%.1f");
            changed |= StatefulImGui::SliderFloat2("MinorTickSize", (float *) &style.MinorTickSize, 0.0f, 2.0f, "%.1f");
            changed |= StatefulImGui::SliderFloat2("MajorGridSize", (float *) &style.MajorGridSize, 0.0f, 2.0f, "%.1f");
            changed |= StatefulImGui::SliderFloat2("MinorGridSize", (float *) &style.MinorGridSize, 0.0f, 2.0f, "%.1f");
            changed |= StatefulImGui::SliderFloat2("PlotDefaultSize", (float *) &style.PlotDefaultSize, 0.0f, 1000, "%.0f");
            changed |= StatefulImGui::SliderFloat2("PlotMinSize", (float *) &style.PlotMinSize, 0.0f, 300, "%.0f");

            ImGui::Text("Plot Padding");
            changed |= StatefulImGui::SliderFloat2("PlotPadding", (float *) &style.PlotPadding, 0.0f, 20.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat2("LabelPadding", (float *) &style.LabelPadding, 0.0f, 20.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat2("LegendPadding", (float *) &style.LegendPadding, 0.0f, 20.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat2("LegendInnerPadding", (float *) &style.LegendInnerPadding, 0.0f, 10.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat2("LegendSpacing", (float *) &style.LegendSpacing, 0.0f, 5.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat2("MousePosPadding", (float *) &style.MousePosPadding, 0.0f, 20.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat2("AnnotationPadding", (float *) &style.AnnotationPadding, 0.0f, 5.0f, "%.0f");
            changed |= StatefulImGui::SliderFloat2("FitPadding", (float *) &style.FitPadding, 0, 0.2f, "%.2f");

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
            for (int i = 0; i < ImPlotCol_COUNT; i++) {
                const char *name = ImPlot::GetStyleColorName(i);
                if (!filter.PassFilter(name)) continue;

                ImGui::PushID(i);
                ImVec4 temp = ImPlot::GetStyleColorVec4(i);
                const bool is_auto = ImPlot::IsColorAuto(i);
                if (!is_auto) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.25f);
                if (ImGui::Button("Auto")) {
                    style.Colors[i] = is_auto ? temp : IMPLOT_AUTO_COL;
                    changed = true;
                }
                if (!is_auto) ImGui::PopStyleVar();
                ImGui::SameLine();
                if (StatefulImGui::ColorEdit4(name, &temp.x, ImGuiColorEditFlags_NoInputs | alpha_flags)) {
                    style.Colors[i] = temp;
                    changed = true;
                }
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

    return changed;
}

bool FlowGridStyleSelector(const char *label, FlowGridStyle &style) {
    static int style_idx = -1;
    if (ImGui::Combo(label, &style_idx, "Dark\0Light\0Classic\0")) {
        switch (style_idx) {
            case 0: FlowGridStyle::StyleColorsDark(style);
                break;
            case 1: FlowGridStyle::StyleColorsLight(style);
                break;
            case 2: FlowGridStyle::StyleColorsClassic(style);
                break;
            default:break;
        }
        return true;
    }
    return false;
}

// Copied from `imgui_demo.cpp`
const ImU64 u64_zero = 0, u64_one = 1, u64_fifty = 50, u64_min = 0, u64_max = ULLONG_MAX / 2, u64_hi_a = ULLONG_MAX / 2 - 100, u64_hi_b = ULLONG_MAX / 2;
static ImU64 u64_v = (ImU64) -1;
#define IM_PRIu64   "llu"

// TODO refactor to `::FlowGrid::StyleEditor::draw()`
bool Windows::StyleEditor::FlowGridStyleEditor() {
    bool changed = false;
    auto &style = ui_s.style.flowgrid;

//    changed |= StatefulImGui::SliderScalar("slider u64 full", ImGuiDataType_U64, &u64_v, &u64_min, &u64_max, "%" IM_PRIu64 " ns");
    changed |= StatefulImGui::SliderScalar("FlashDurationNs", ImGuiDataType_U64, &style.FlashDurationNs, &u64_min, &u64_max, "%" IM_PRIu64 " ns");
    changed |= FlowGridStyleSelector("Colors##Selector", style);

    if (ImGui::BeginTabBar("##FlowGridStyleEditor")) {
        if (ImGui::BeginTabItem("Colors")) {
            // TODO DRY up this part across style editors
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
            for (int i = 0; i < FlowGridCol_COUNT; i++) {
                const char *name = FlowGridStyle::GetColorName(i);
                if (!filter.PassFilter(name)) continue;

                ImGui::PushID(i);
                ImVec4 temp = style.Colors[i];
                ImGui::SameLine();
                if (StatefulImGui::ColorEdit4(name, &temp.x, ImGuiColorEditFlags_NoInputs | alpha_flags)) {
                    style.Colors[i] = temp;
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::PopItemWidth(); // TODO can we delete this?
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    return changed;
}

void Windows::StyleEditor::draw() {
    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("FlowGrid")) {
            if (FlowGridStyleEditor()) q(set_flowgrid_style{ui_s.style.flowgrid});
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ImGui")) {
            if (ImGuiStyleEditor()) q(set_imgui_style{ui_s.style.imgui});
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("ImPlot")) {
            if (ImPlotStyleEditor()) {
                ImPlot::BustItemCache();
                q(set_implot_style{ui_s.style.implot});
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
