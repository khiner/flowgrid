#include "../windows.h"
#include "implot.h"
#include "implot_internal.h"
#include "../../context.h"
#include "../../stateful_imgui.h"
#include "../../imgui_helpers.h"

// From `implot_demo.cpp`
bool ShowStyleSelector(const char *label, ImPlotStyle *dst) {
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


void ImPlotStyleEditor::draw(Window &) {
    static bool changed = false;

    auto &style = ui_s.ui.implot_style;
    changed |= ShowStyleSelector("Colors##Selector", &style);

    if (ImGui::BeginTabBar("##StyleEditor")) {
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

    if (changed) {
        ImPlot::BustItemCache();
        q.enqueue(set_implot_style{style});
    }
    changed = false;
}
