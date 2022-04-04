#include "imgui_windows.h"
#include "imgui.h"
#include "../context.h"
#include "imgui_internal.h"
#include "../stateful_imgui.h"

void ImGuiWindows::Metrics::draw(Window &window) {
    ImGui::ShowMetricsWindow(&window.visible);
}

void ImGuiWindows::Demo::draw(Window &window) {
    ImGui::ShowDemoWindow(&window.visible);
}

// From `imgui_demo.cpp`
// Helper to display a little (?) mark which shows a tooltip when hovered.
// In your own code you may want to display an actual icon if you are using a merged icon fonts (see docs/FONTS.md)
static void HelpMarker(const char *desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

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

//    ImGui::ShowStyleEditor(&ui_s.ui.style);
void ImGuiWindows::StyleEditor::draw(Window &) {
    static bool changed = false;

    auto &style = ui_s.ui.style;
    ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.50f);

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

    if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
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
                if (!filter.PassFilter(name))
                    continue;
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

            changed |= StatefulImGui::DragFloat("Global Alpha", &style.Alpha, 0.005f, 0.20f, 1.0f,
                "%.2f"); // Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets). But application code could have a toggle to switch between zero and non-zero.
            changed |= StatefulImGui::DragFloat("Disabled Alpha", &style.DisabledAlpha, 0.005f, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            HelpMarker("Additional alpha multiplier for disabled items (multiply over current value of Alpha).");
            ImGui::PopItemWidth();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::PopItemWidth();

    if (changed) q.enqueue(set_style{style});
    changed = false;
}
