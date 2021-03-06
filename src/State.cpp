#include "Context.h"

#include "imgui_memory_editor.h"

#include "FileDialog/ImGuiFileDialogDemo.h"
#include "ImGuiFileDialog.h"
#include "UI/Widgets.h"
#include <fstream>

using namespace ImGui;
using namespace fg;

/**
 * Inspired by [`lager`](https://sinusoid.es/lager/architecture.html#reducer), but only the action-visitor pattern remains.
 */
void State::update(const Action &action) {
    std::visit(visitor{
        [&](const show_open_project_dialog &) { file.dialog = {"Choose file", AllProjectExtensionsDelimited, "."}; },
        [&](const show_save_project_dialog &) { file.dialog = {"Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}; },
        [&](const show_open_faust_file_dialog &) { file.dialog = {"Choose file", FaustDspFileExtension, "."}; },
        [&](const show_save_faust_file_dialog &) { file.dialog = {"Choose file", FaustDspFileExtension, ".", "my_dsp", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}; },

        [&](const open_file_dialog &a) {
            file.dialog = a.dialog;
            file.dialog.visible = true;
        },
        [&](const close_file_dialog &) { file.dialog.visible = false; },

        [&](const set_imgui_settings &a) { imgui_settings = a.settings; },
        [&](const set_imgui_color_style &a) {
            auto *dst = &style.imgui;
            switch (a.id) {
                case 0: ImGui::StyleColorsDark(dst);
                    break;
                case 1: ImGui::StyleColorsLight(dst);
                    break;
                case 2: ImGui::StyleColorsClassic(dst);
                    break;
            }
        },
        [&](const set_implot_color_style &a) {
            auto *dst = &style.implot;
            switch (a.id) {
                case 0: ImPlot::StyleColorsAuto(dst);
                    break;
                case 1: ImPlot::StyleColorsClassic(dst);
                    break;
                case 2: ImPlot::StyleColorsDark(dst);
                    break;
                case 3: ImPlot::StyleColorsLight(dst);
                    break;
            }
        },
        [&](const set_flowgrid_color_style &a) {
            auto &dst = style.flowgrid;
            switch (a.id) {
                case 0: FlowGridStyle::StyleColorsDark(dst);
                    break;
                case 1: FlowGridStyle::StyleColorsLight(dst);
                    break;
                case 2: FlowGridStyle::StyleColorsClassic(dst);
                    break;
                default:break;
            }
        },

        [&](const close_window &a) { window_named.at(a.name).get().visible = false; },
        [&](const toggle_window &a) { window_named.at(a.name).get().visible = !window_named.at(a.name).get().visible; },

        [&](const toggle_state_viewer_auto_select &) { state_viewer.auto_select = !state_viewer.auto_select; },
        [&](const set_state_viewer_label_mode &a) { state_viewer.label_mode = a.label_mode; },

        // Audio
        [&](const open_faust_file &a) { audio.faust.code = ::File::read(a.path); },
        [&](const set_faust_code &a) { audio.faust.code = a.text; },
        [&](const set_audio_sample_rate &a) { audio.settings.sample_rate = a.sample_rate; },

        [&](const set_ui_running &a) { processes.ui.running = a.running; },

        [&](const close_application &) {
            processes.ui.running = false;
            processes.audio.running = false;
        },

        [&](const auto &) {}, // All actions that don't directly update state (e.g. undo/redo & open/load-project)
    }, action);
}

ImGuiSettings::ImGuiSettings(ImGuiContext *c) {
    ImGui::SaveIniSettingsToMemory(); // Populates the `Settings` context members
    nodes = c->DockContext.NodesSettings; // already an ImVector
    // Convert `ImChunkStream`s to `ImVector`s.
    for (auto *ws = c->SettingsWindows.begin(); ws != nullptr; ws = c->SettingsWindows.next_chunk(ws)) {
        windows.push_back(*ws);
    }
    for (auto *ts = c->SettingsTables.begin(); ts != nullptr; ts = c->SettingsTables.next_chunk(ts)) {
        tables.push_back(*ts);
    }
}

// Copied from `imgui.cpp`
static void ApplyWindowSettings(ImGuiWindow *window, ImGuiWindowSettings *settings) {
    if (!window) return; // TODO log

    const ImGuiViewport *main_viewport = ImGui::GetMainViewport();
    window->ViewportPos = main_viewport->Pos;
    if (settings->ViewportId) {
        window->ViewportId = settings->ViewportId;
        window->ViewportPos = ImVec2(settings->ViewportPos.x, settings->ViewportPos.y);
    }
    window->Pos = ImFloor(ImVec2(settings->Pos.x + window->ViewportPos.x, settings->Pos.y + window->ViewportPos.y));
    if (settings->Size.x > 0 && settings->Size.y > 0)
        window->Size = window->SizeFull = ImFloor(ImVec2(settings->Size.x, settings->Size.y));
    window->Collapsed = settings->Collapsed;
    window->DockId = settings->DockId;
    window->DockOrder = settings->DockOrder;
}

void ImGuiSettings::populate_context(ImGuiContext *ctx) const {
    /** Clear **/
    ImGui::DockSettingsHandler_ClearAll(ctx, nullptr);

    /** Apply **/
    for (auto ws: windows) ApplyWindowSettings(ImGui::FindWindowByID(ws.ID), &ws);

    ctx->DockContext.NodesSettings = nodes; // already an ImVector
    ImGui::DockSettingsHandler_ApplyAll(ctx, nullptr);

    /** Other housekeeping to emulate `ImGui::LoadIniSettingsFromMemory` **/
    ctx->SettingsLoaded = true;
    ctx->SettingsDirty = false;
}

void Audio::Settings::draw() const {
    Checkbox("/processes/audio/running");
    Checkbox("/audio/settings/muted");
}

typedef int JsonTreeNodeFlags;
enum JsonTreeNodeFlags_ {
    JsonTreeNodeFlags_None = 0,
    JsonTreeNodeFlags_Highlighted = 1 << 0,
    JsonTreeNodeFlags_Disabled = 1 << 1,
};

bool JsonTreeNode(const char *label, JsonTreeNodeFlags flags) {
    bool highlighted = flags & JsonTreeNodeFlags_Highlighted;
    bool disabled = flags & JsonTreeNodeFlags_Disabled;

    if (disabled) BeginDisabled();
    if (highlighted) PushStyleColor(ImGuiCol_Text, s.style.flowgrid.Colors[FlowGridCol_HighlightText]);
    bool is_open = TreeNode(label);
    if (highlighted) PopStyleColor();
    if (disabled) EndDisabled();

    return is_open;
}

bool is_number(const string &str) {
    return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

enum ColorPaths_ {
    ColorPaths_ImGui,
    ColorPaths_ImPlot,
    ColorPaths_FlowGrid,
};
//using ColorPaths = int;

static const fs::path color_paths[] = { // addressable by `ColorPaths`
    "/style/imgui/Colors",
    "/style/implot/Colors",
    "/style/flowgrid/Colors",
};

static void show_json_state_value_node(const string &key, const json &value, const fs::path &path) {
    const bool auto_select = s.state_viewer.auto_select;
    const bool annotate_enabled = s.state_viewer.label_mode == StateViewer::LabelMode::annotated;

    const string &file_name = path.filename();
    const bool is_array_item = is_number(file_name);
    const int array_index = is_array_item ? std::stoi(file_name) : -1;
    const bool is_color = string(path).find("Colors") != string::npos && is_array_item;
    const bool is_imgui_color = path.parent_path() == color_paths[ColorPaths_ImGui];
    const bool is_implot_color = path.parent_path() == color_paths[ColorPaths_ImPlot];
    const bool is_flowgrid_color = path.parent_path() == color_paths[ColorPaths_FlowGrid];
    const auto &name = annotate_enabled ?
                       (is_imgui_color ?
                        GetStyleColorName(array_index) : is_implot_color ? ImPlot::GetStyleColorName(array_index) :
                                                         is_flowgrid_color ? FlowGridStyle::GetColorName(array_index) :
                                                         is_array_item ? file_name : key) : key;

    if (auto_select) {
        const auto &update_paths = c.state_stats.most_recent_update_paths;
        const auto is_ancestor_path = [path](const string &candidate_path) { return candidate_path.rfind(path, 0) == 0; };
        const bool was_recently_updated = std::find_if(update_paths.begin(), update_paths.end(), is_ancestor_path) != update_paths.end();
        SetNextItemOpen(was_recently_updated);
    }

    JsonTreeNodeFlags node_flags = JsonTreeNodeFlags_None;
    if (annotate_enabled && is_color) node_flags |= JsonTreeNodeFlags_Highlighted;
    if (auto_select) node_flags |= JsonTreeNodeFlags_Disabled;

    // Tree acts like a histogram, where rect length corresponds to relative update frequency, with `width => most frequently updated`.
    // Background color of nodes flashes on update.
    if (c.state_stats.update_times_for_state_path.contains(path)) {
        const auto &update_times = c.state_stats.update_times_for_state_path.at(path);

        const ImVec2 row_min = {GetWindowPos().x, GetCursorScreenPos().y};
        const float item_w = GetWindowWidth();
        const ImVec2 row_max = {row_min.x + item_w, row_min.y + GetFontSize()};
        const float max_ratio = float(update_times.size()) / float(c.state_stats.max_num_updates);
        GetWindowDrawList()->AddRectFilled(
            row_min, {row_min.x + item_w * max_ratio, row_max.y},
            ImColor(GetStyleColorVec4(ImGuiCol_PlotHistogram)),
            0.0f, ImDrawFlags_None
        );

        // Flash background on update
        const auto most_recent_update_time = update_times.back();
        const fsec flash_remaining_sec = Clock::now() - most_recent_update_time;
        const float flash_complete_ratio = flash_remaining_sec.count() / s.style.flowgrid.FlashDurationSec;
        auto flash_color = s.style.flowgrid.Colors[FlowGridCol_Flash];
        flash_color.w = std::max(0.0f, 1 - flash_complete_ratio);
        GetWindowDrawList()->AddRectFilled(row_min, row_max, ImColor(flash_color), 0.0f, ImDrawFlags_None);

        // TODO indicate relative update-recency
    }

    if (value.is_null()) {
        Text("null");
    } else if (value.is_object()) {
        if (JsonTreeNode(name.c_str(), node_flags)) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                show_json_state_value_node(it.key(), it.value(), path / it.key());
            }
            TreePop();
        }
    } else if (value.is_array()) {
        if (JsonTreeNode(name.c_str(), node_flags)) {
            int i = 0;
            for (const auto &it: value) {
                show_json_state_value_node(std::to_string(i), it, path / std::to_string(i));
                i++;
            }
            TreePop();
        }
    } else {
        Text("%s : %s", name.c_str(), value.dump().c_str());
    }
}

void StateMemoryEditor::draw() const {
    static MemoryEditor memory_editor;
    static bool first_render{true};
    if (first_render) {
        memory_editor.OptShowDataPreview = true;
        first_render = false;
    }

    void *mem_data{&c.state};
    size_t mem_size{sizeof(c.state)};
    memory_editor.DrawContents(mem_data, mem_size);
}

void StatePathUpdateFrequency::draw() const {
    if (c.state_stats.update_times_for_state_path.empty()) {
        Text("No state updates yet.");
        return;
    }

    auto &[labels, values] = c.state_stats.path_update_frequency_plottable;

    if (ImPlot::BeginPlot("Path update frequency", {-1, float(labels.size()) * 30.0f + 60.0f}, ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Number of updates", nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert);

        // Hack to allow `SetupAxisTicks` without breaking on assert `n_ticks > 1`: Just add an empty label and only plot one value.
        if (labels.size() == 1) labels.emplace_back("");

        ImPlot::PushStyleColor(ImPlotCol_Fill, GetStyleColorVec4(ImGuiCol_PlotHistogram));
        ImPlot::SetupAxisTicks(ImAxis_X1, 0, double(c.state_stats.max_num_updates), int(c.state_stats.max_num_updates) + 1, nullptr, false);
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, double(labels.size() - 1), int(labels.size()), labels.data(), false);
        ImPlot::PlotBars("Number of updates", values.data(), int(values.size()), 0.75, 0, ImPlotBarsFlags_Horizontal);
        ImPlot::EndPlot();
        ImPlot::PopStyleColor();
    }
}

static const string label_help = "The raw JSON state doesn't store keys for all items.\n"
                                 "For example, the main `ui.style.colors` state is a list.\n\n"
                                 "'Annotated' mode shows (highlighted) labels for such state items.\n"
                                 "'Raw' mode shows the state exactly as it is in the raw JSON state.";
static const string auto_select_help = "When auto-select is enabled, state changes automatically open.\n"
                                       "The state viewer to the changed state node(s), closing all other state nodes.\n"
                                       "State menu items can only be opened or closed manually if auto-select is disabled.";

void StateViewer::draw() const {
    if (BeginMenuBar()) {
        if (BeginMenu("Settings")) {
            if (MenuItemWithHelp("Auto-select", auto_select_help.c_str(), nullptr, s.state_viewer.auto_select)) {
                q(toggle_state_viewer_auto_select{});
            }
            if (BeginMenuWithHelp("Label mode", label_help.c_str())) {
                if (MenuItem("Annotated", nullptr, label_mode == LabelMode::annotated)) {
                    q(set_state_viewer_label_mode{LabelMode::annotated});
                } else if (MenuItem("Raw", nullptr, label_mode == LabelMode::raw)) {
                    q(set_state_viewer_label_mode{LabelMode::raw});
                }
                EndMenu();
            }
            EndMenu();
        }
        EndMenuBar();
    }

    show_json_state_value_node("State", sj, "/");
}

void Demo::draw() const {
    if (BeginTabBar("##demos")) {
        if (BeginTabItem("ImGui")) {
            ShowDemo();
            EndTabItem();
        }
        if (BeginTabItem("ImPlot")) {
            ShowDemo();
            EndTabItem();
        }
        if (BeginTabItem("ImGuiFileDialog")) {
            IGFD::ShowDemo();
            EndTabItem();
        }
        EndTabBar();
    }
}

namespace FlowGrid {

void ShowJsonPatchOpMetrics(const JsonPatchOp &patch_op) {
    BulletText("Path: %s", patch_op.path.c_str());
    BulletText("Op: %s", json(patch_op.op).dump().c_str());
    if (patch_op.value.has_value()) {
        BulletText("Value: %s", patch_op.value.value().dump().c_str());
    }
    if (patch_op.from.has_value()) {
        BulletText("From: %s", patch_op.from.value().c_str());
    }
}

void ShowJsonPatchMetrics(const JsonPatch &patch) {
    if (patch.size() == 1) {
        ShowJsonPatchOpMetrics(patch[0]);
    } else {
        for (size_t i = 0; i < patch.size(); i++) {
            if (TreeNodeEx(std::to_string(i).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ShowJsonPatchOpMetrics(patch[i]);
                TreePop();
            }
        }
    }
}

void ShowDiffMetrics(const BidirectionalStateDiff &diff) {
    if (diff.action_names.size() == 1) {
        BulletText("Action name: %s", (*diff.action_names.begin()).c_str());
    } else {
        if (TreeNode("Actions", "%lu actions", diff.action_names.size())) {
            for (const auto &action_name: diff.action_names) {
                BulletText("%s", action_name.c_str());
            }
            TreePop();
        }
    }
    if (TreeNode("Forward diff")) {
        ShowJsonPatchMetrics(diff.forward_patch);
        TreePop();
    }
    if (TreeNode("Reverse diff")) {
        ShowJsonPatchMetrics(diff.reverse_patch);
        TreePop();
    }

    BulletText("Time: %s", fmt::format("{}\n", diff.system_time).c_str());
    TreePop();
}

void ShowMetrics(bool show_relative_paths) {
    Text("Gesturing: %s", c.gesturing ? "true" : "false");

    const bool has_diffs = !c.diffs.empty();
    if (!has_diffs) BeginDisabled();
    if (TreeNodeEx("Diffs", ImGuiTreeNodeFlags_DefaultOpen, "Diffs (Count: %lu, Current index: %d)", c.diffs.size(), c.current_diff_index)) {
        for (size_t i = 0; i < c.diffs.size(); i++) {
            if (TreeNodeEx(std::to_string(i).c_str(), int(i) == c.current_diff_index ? (ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_DefaultOpen) : ImGuiTreeNodeFlags_None)) {
                ShowDiffMetrics(c.diffs[i]);
            }
        }
        TreePop();
    }
    if (!has_diffs) EndDisabled();

    const bool has_recently_opened_paths = !c.preferences.recently_opened_paths.empty();
    if (TreeNodeEx("Preferences", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (SmallButton("Clear")) c.clear_preferences();
        SameLine();
        Checkbox("/metrics/show_relative_paths");

        if (!has_recently_opened_paths) BeginDisabled();
        if (TreeNodeEx("Recently opened paths", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const auto &recently_opened_path: c.preferences.recently_opened_paths) {
                BulletText("%s", (show_relative_paths ? fs::relative(recently_opened_path) : recently_opened_path).c_str());
            }
            TreePop();
        }
        if (!has_recently_opened_paths) EndDisabled();

        TreePop();
    }
    Text("Action variant size: %lu bytes", sizeof(Action));
    SameLine();
    HelpMarker("All actions are internally stored in an `std::variant`, which must be large enough to hold its largest type. "
               "Thus, it's important to keep action data small.");
}

}

void Metrics::draw() const {
    if (BeginTabBar("##metrics")) {
        if (BeginTabItem("FlowGrid")) {
            ShowMetrics(show_relative_paths);
            EndTabItem();
        }
        if (BeginTabItem("ImGui")) {
            ShowMetrics();
            EndTabItem();
        }
        if (BeginTabItem("ImPlot")) {
            ImPlot::ShowMetrics();
            EndTabItem();
        }
        EndTabBar();
    }
}

void Tools::draw() const {
    if (BeginTabBar("##tools")) {
        if (BeginTabItem("ImGui")) {
            if (BeginTabBar("##imgui_tools")) {
                if (BeginTabItem("Debug log")) {
                    ShowDebugLog();
                    EndTabItem();
                }
                EndTabBar();
            }
            EndTabItem();
        }
        EndTabBar();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Style editors
//-----------------------------------------------------------------------------

void ShowColorEditor(const char *path, int color_count, const std::function<const char *(int)> &GetColorName) {
    if (BeginTabItem("Colors")) {
        static ImGuiTextFilter filter;
        filter.Draw("Filter colors", GetFontSize() * 16);

        static ImGuiColorEditFlags alpha_flags = 0;
        if (RadioButton("Opaque", alpha_flags == ImGuiColorEditFlags_None)) { alpha_flags = ImGuiColorEditFlags_None; }
        SameLine();
        if (RadioButton("Alpha", alpha_flags == ImGuiColorEditFlags_AlphaPreview)) { alpha_flags = ImGuiColorEditFlags_AlphaPreview; }
        SameLine();
        if (RadioButton("Both", alpha_flags == ImGuiColorEditFlags_AlphaPreviewHalf)) { alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf; }
        SameLine();
        HelpMarker(
            "In the color list:\n"
            "Left-click on color square to open color picker,\n"
            "Right-click to open edit options menu.");

        BeginChild("##colors", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NavFlattened);
        PushItemWidth(-160);
        for (int i = 0; i < color_count; i++) {
            const char *name = GetColorName(i);
            if (!filter.PassFilter(name)) continue;

            PushID(i);
            ColorEdit4((JsonPath(path) / i).to_string().c_str(), ImGuiColorEditFlags_AlphaBar | alpha_flags);
            SameLine(0.0f, s.style.imgui.ItemInnerSpacing.x);
            TextUnformatted(name);
            PopID();
        }
        PopItemWidth();
        EndChild();

        EndTabItem();
    }
}

// Returns `true` if style changes.
void Style::ImGuiStyleEditor() {
    static int style_idx = -1;
    if (Combo("Colors##Selector", &style_idx, "Dark\0Light\0Classic\0")) q(set_imgui_color_style{style_idx});
//    ShowFontSelector("Fonts##Selector"); // TODO

    // Simplified Settings (expose floating-pointer border sizes as boolean representing 0.0f or 1.0f)
    // TODO match with imgui
    if (SliderFloat("/style/imgui/FrameRounding", 0.0f, 12.0f, "%.0f")) {
        q(set_value{"/style/imgui/GrabRounding", s.style.imgui.FrameRounding}); // Make GrabRounding always the same value as FrameRounding
    }
    {
        bool border = s.style.imgui.WindowBorderSize > 0.0f;
        if (Checkbox("WindowBorder", &border)) q(set_value{"/style/imgui/WindowBorderSize", border ? 1.0f : 0.0f});
    }
    SameLine();
    {
        bool border = s.style.imgui.FrameBorderSize > 0.0f;
        if (Checkbox("FrameBorder", &border)) q(set_value{"/style/imgui/FrameBorderSize", border ? 1.0f : 0.0f});
    }
    SameLine();
    {
        bool border = s.style.imgui.PopupBorderSize > 0.0f;
        if (Checkbox("PopupBorder", &border)) q(set_value{"/style/imgui/PopupBorderSize", border ? 1.0f : 0.0f});
    }

    Separator();

    if (BeginTabBar("##ImGuiStyleEditor", ImGuiTabBarFlags_None)) {
        if (BeginTabItem("Sizes")) {
            Text("Main");
            SliderFloat2("/style/imgui/WindowPadding", 0.0f, 20.0f, "%.0f");
            SliderFloat2("/style/imgui/FramePadding", 0.0f, 20.0f, "%.0f");
            SliderFloat2("/style/imgui/CellPadding", 0.0f, 20.0f, "%.0f");
            SliderFloat2("/style/imgui/ItemSpacing", 0.0f, 20.0f, "%.0f");
            SliderFloat2("/style/imgui/ItemInnerSpacing", 0.0f, 20.0f, "%.0f");
            SliderFloat2("/style/imgui/TouchExtraPadding", 0.0f, 10.0f, "%.0f");
            SliderFloat("/style/imgui/IndentSpacing", 0.0f, 30.0f, "%.0f");
            SliderFloat("/style/imgui/ScrollbarSize", 1.0f, 20.0f, "%.0f");
            SliderFloat("/style/imgui/GrabMinSize", 1.0f, 20.0f, "%.0f");
            Text("Borders");
            SliderFloat("/style/imgui/WindowBorderSize", 0.0f, 1.0f, "%.0f");
            SliderFloat("/style/imgui/ChildBorderSize", 0.0f, 1.0f, "%.0f");
            SliderFloat("/style/imgui/PopupBorderSize", 0.0f, 1.0f, "%.0f");
            SliderFloat("/style/imgui/FrameBorderSize", 0.0f, 1.0f, "%.0f");
            SliderFloat("/style/imgui/TabBorderSize", 0.0f, 1.0f, "%.0f");
            Text("Rounding");
            SliderFloat("/style/imgui/WindowRounding", 0.0f, 12.0f, "%.0f");
            SliderFloat("/style/imgui/ChildRounding", 0.0f, 12.0f, "%.0f");
            SliderFloat("/style/imgui/FrameRounding", 0.0f, 12.0f, "%.0f");
            SliderFloat("/style/imgui/PopupRounding", 0.0f, 12.0f, "%.0f");
            SliderFloat("/style/imgui/ScrollbarRounding", 0.0f, 12.0f, "%.0f");
            SliderFloat("/style/imgui/GrabRounding", 0.0f, 12.0f, "%.0f");
            SliderFloat("/style/imgui/LogSliderDeadzone", 0.0f, 12.0f, "%.0f");
            SliderFloat("/style/imgui/TabRounding", 0.0f, 12.0f, "%.0f");
            Text("Alignment");
            SliderFloat2("/style/imgui/WindowTitleAlign", 0.0f, 1.0f, "%.2f");
            Combo("/style/imgui/WindowMenuButtonPosition", "None\0Left\0Right\0");
            Combo("/style/imgui/ColorButtonPosition", "Left\0Right\0");
            SliderFloat2("/style/imgui/ButtonTextAlign", 0.0f, 1.0f, "%.2f");
            SameLine();
            HelpMarker("Alignment applies when a button is larger than its text content.");
            SliderFloat2("/style/imgui/SelectableTextAlign", 0.0f, 1.0f, "%.2f");
            SameLine();
            HelpMarker("Alignment applies when a selectable is larger than its text content.");
            Text("Safe Area Padding");
            SameLine();
            HelpMarker("Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured).");
            SliderFloat2("/style/imgui/DisplaySafeAreaPadding", 0.0f, 30.0f, "%.0f");
            EndTabItem();
        }

        ShowColorEditor("/style/imgui/Colors", ImGuiCol_COUNT, GetStyleColorName);

//        if (BeginTabItem("Fonts")) {
//            ImGuiIO &io = GetIO();
//            ImFontAtlas *atlas = io.Fonts;
//            HelpMarker("Read FAQ and docs/FONTS.md for details on font loading.");
//            ShowFontAtlas(atlas);
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
//            PushItemWidth(GetFontSize() * 8);
//            if (DragFloat("window scale", &window_scale, 0.005f, MIN_SCALE, MAX_SCALE, "%.2f", ImGuiSliderFlags_AlwaysClamp)) // Scale only this window
//                SetWindowFontScale(window_scale);
//            DragFloat("global scale", &io.FontGlobalScale, 0.005f, MIN_SCALE, MAX_SCALE, "%.2f", ImGuiSliderFlags_AlwaysClamp); // Scale everything
//            PopItemWidth();
//
//            EndTabItem();
//        }

        if (BeginTabItem("Rendering")) {
            Checkbox("/style/imgui/AntiAliasedLines", "Anti-aliased lines");
            SameLine();
            HelpMarker("When disabling anti-aliasing lines, you'll probably want to disable borders in your style as well.");

            Checkbox("/style/imgui/AntiAliasedLinesUseTex", "Anti-aliased lines use texture");
            SameLine();
            HelpMarker("Faster lines using texture data. Require backend to render with bilinear filtering (not point/nearest filtering).");

            Checkbox("/style/imgui/AntiAliasedFill", "Anti-aliased fill");
            PushItemWidth(GetFontSize() * 8);
            DragFloat("/style/imgui/CurveTessellationTol", 0.02f, 0.10f, 10.0f, "%.2f", ImGuiSliderFlags_None, "Curve Tessellation Tolerance");

            // When editing the "Circle Segment Max Error" value, draw a preview of its effect on auto-tessellated circles.
            DragFloat("/style/imgui/CircleTessellationMaxError", 0.005f, 0.10f, 5.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            if (IsItemActive()) {
                SetNextWindowPos(GetCursorScreenPos());
                BeginTooltip();
                TextUnformatted("(R = radius, N = number of segments)");
                Spacing();
                ImDrawList *draw_list = GetWindowDrawList();
                const float min_widget_width = CalcTextSize("N: MMM\nR: MMM").x;
                for (int n = 0; n < 8; n++) {
                    const float RAD_MIN = 5.0f;
                    const float RAD_MAX = 70.0f;
                    const float rad = RAD_MIN + (RAD_MAX - RAD_MIN) * (float) n / (8.0f - 1.0f);

                    BeginGroup();

                    Text("R: %.f\nN: %d", rad, draw_list->_CalcCircleAutoSegmentCount(rad));

                    const float canvas_width = ImMax(min_widget_width, rad * 2.0f);
                    const float offset_x = floorf(canvas_width * 0.5f);
                    const float offset_y = floorf(RAD_MAX);

                    const ImVec2 p1 = GetCursorScreenPos();
                    draw_list->AddCircle(ImVec2(p1.x + offset_x, p1.y + offset_y), rad, GetColorU32(ImGuiCol_Text));
                    Dummy(ImVec2(canvas_width, RAD_MAX * 2));

                    EndGroup();
                    SameLine();
                }
                EndTooltip();
            }
            SameLine();
            HelpMarker("When drawing circle primitives with \"num_segments == 0\" tesselation will be calculated automatically.");

            // Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets). But application code could have a toggle to switch between zero and non-zero.
            DragFloat("/style/imgui/Alpha", 0.005f, 0.20f, 1.0f, "%.2f");
            DragFloat("/style/imgui/DisabledAlpha", 0.005f, 0.0f, 1.0f, "%.2f");
            SameLine();
            HelpMarker("Additional alpha multiplier for disabled items (multiply over current value of Alpha).");
            PopItemWidth();

            EndTabItem();
        }

        EndTabBar();
    }
}

void Style::ImPlotStyleEditor() {
    static int style_idx = -1;
    if (Combo("Colors##Selector", &style_idx, "Auto\0Classic\0Dark\0Light\0")) q(set_implot_color_style{style_idx});

    if (BeginTabBar("##ImPlotStyleEditor")) {
        if (BeginTabItem("Variables")) {
            Text("Item Styling");
            SliderFloat("/style/implot/LineWeight", 0.0f, 5.0f, "%.1f");
            SliderFloat("/style/implot/MarkerSize", 2.0f, 10.0f, "%.1f");
            SliderFloat("/style/implot/MarkerWeight", 0.0f, 5.0f, "%.1f");
            SliderFloat("/style/implot/FillAlpha", 0.0f, 1.0f, "%.2f");
            SliderFloat("/style/implot/ErrorBarSize", 0.0f, 10.0f, "%.1f");
            SliderFloat("/style/implot/ErrorBarWeight", 0.0f, 5.0f, "%.1f");
            SliderFloat("/style/implot/DigitalBitHeight", 0.0f, 20.0f, "%.1f");
            SliderFloat("/style/implot/DigitalBitGap", 0.0f, 20.0f, "%.1f");

            Text("Plot Styling");
            SliderFloat("/style/implot/PlotBorderSize", 0.0f, 2.0f, "%.0f");
            SliderFloat("/style/implot/MinorAlpha", 0.0f, 1.0f, "%.2f");
            SliderFloat2("/style/implot/MajorTickLen", 0.0f, 20.0f, "%.0f");
            SliderFloat2("/style/implot/MinorTickLen", 0.0f, 20.0f, "%.0f");
            SliderFloat2("/style/implot/MajorTickSize", 0.0f, 2.0f, "%.1f");
            SliderFloat2("/style/implot/MinorTickSize", 0.0f, 2.0f, "%.1f");
            SliderFloat2("/style/implot/MajorGridSize", 0.0f, 2.0f, "%.1f");
            SliderFloat2("/style/implot/MinorGridSize", 0.0f, 2.0f, "%.1f");
            SliderFloat2("/style/implot/PlotDefaultSize", 0.0f, 1000, "%.0f");
            SliderFloat2("/style/implot/PlotMinSize", 0.0f, 300, "%.0f");

            Text("Plot Padding");
            SliderFloat2("/style/implot/PlotPadding", 0.0f, 20.0f, "%.0f");
            SliderFloat2("/style/implot/LabelPadding", 0.0f, 20.0f, "%.0f");
            SliderFloat2("/style/implot/LegendPadding", 0.0f, 20.0f, "%.0f");
            SliderFloat2("/style/implot/LegendInnerPadding", 0.0f, 10.0f, "%.0f");
            SliderFloat2("/style/implot/LegendSpacing", 0.0f, 5.0f, "%.0f");
            SliderFloat2("/style/implot/MousePosPadding", 0.0f, 20.0f, "%.0f");
            SliderFloat2("/style/implot/AnnotationPadding", 0.0f, 5.0f, "%.0f");
            SliderFloat2("/style/implot/FitPadding", 0, 0.2f, "%.2f");

            EndTabItem();
        }
        if (BeginTabItem("Colors")) {
            static ImGuiTextFilter filter;
            filter.Draw("Filter colors", GetFontSize() * 16);

            static ImGuiColorEditFlags alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf;
            if (RadioButton("Opaque", alpha_flags == ImGuiColorEditFlags_None)) { alpha_flags = ImGuiColorEditFlags_None; }
            SameLine();
            if (RadioButton("Alpha", alpha_flags == ImGuiColorEditFlags_AlphaPreview)) { alpha_flags = ImGuiColorEditFlags_AlphaPreview; }
            SameLine();
            if (RadioButton("Both", alpha_flags == ImGuiColorEditFlags_AlphaPreviewHalf)) { alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf; }
            SameLine();
            HelpMarker(
                "In the color list:\n"
                "Left-click on colored square to open color picker,\n"
                "Right-click to open edit options menu.");

            Separator();
            PushItemWidth(-160);
            const auto colors_path = JsonPath("/style/implot/Colors");
            for (int i = 0; i < ImPlotCol_COUNT; i++) {
                const char *name = ImPlot::GetStyleColorName(i);
                if (!filter.PassFilter(name)) continue;

                PushID(i);
                ImVec4 temp = ImPlot::GetStyleColorVec4(i);
                const bool is_auto = ImPlot::IsColorAuto(i);
                if (!is_auto) PushStyleVar(ImGuiStyleVar_Alpha, 0.25f);
                if (Button("Auto")) q(set_value{colors_path / i, is_auto ? temp : IMPLOT_AUTO_COL});
                if (!is_auto) PopStyleVar();
                SameLine();
                ColorEdit4((colors_path / i).to_string().c_str(), ImGuiColorEditFlags_NoInputs | alpha_flags, name);
                PopID();
            }
            PopItemWidth();
            Separator();
            Text("Colors that are set to Auto (i.e. IMPLOT_AUTO_COL) will\n"
                 "be automatically deduced from your ImGui style or the\n"
                 "current ImPlot Colormap. If you want to style individual\n"
                 "plot items, use Push/PopStyleColor around its function.");
            EndTabItem();
        }
        // TODO re-implement colormaps statefully
        EndTabBar();
    }
}

void Style::FlowGridStyleEditor() {
    SliderFloat("/style/flowgrid/FlashDurationSec", FlashDurationSecMin, FlashDurationSecMax, "%.3f s");
    static int style_idx = -1;
    if (Combo("Colors##Selector", &style_idx, "Dark\0Light\0Classic\0")) q(set_flowgrid_color_style{style_idx});

    if (BeginTabBar("##FlowGridStyleEditor")) {
        ShowColorEditor("/style/flowgrid/Colors", FlowGridCol_COUNT, FlowGridStyle::GetColorName);
        EndTabBar();
    }
}

void Style::draw() const {
    if (BeginTabBar("##style")) {
        if (BeginTabItem("FlowGrid")) {
            FlowGridStyleEditor();
            EndTabItem();
        }
        if (BeginTabItem("ImGui")) {
            ImGuiStyleEditor();
            EndTabItem();
        }
        if (BeginTabItem("ImPlot")) {
            ImPlotStyleEditor();
            EndTabItem();
        }
        EndTabBar();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] File
//-----------------------------------------------------------------------------

static auto *file_dialog = ImGuiFileDialog::Instance();
static const string file_dialog_key = "FileDialog";

void File::Dialog::draw() const {
    if (visible) {
        // `OpenDialog` is a no-op if it's already open, so it's safe to call every frame.
        file_dialog->OpenDialog(file_dialog_key, title, filters.c_str(), file_path, default_file_name, max_num_selections, nullptr, flags);

        // TODO need to get custom vecs with math going
        const ImVec2 min_dialog_size = {GetMainViewport()->Size.x / 2.0f, GetMainViewport()->Size.y / 2.0f};
        if (file_dialog->Display(file_dialog_key, ImGuiWindowFlags_NoCollapse, min_dialog_size)) {
            if (file_dialog->IsOk()) {
                const fs::path &file_path = file_dialog->GetFilePathName();
                const string &extension = file_path.extension();
                if (AllProjectExtensions.find(extension) != AllProjectExtensions.end()) {
                    // TODO provide an option to save with undo state.
                    //   This file format would be a json list of diffs.
                    //   The file would generally be larger, and the load time would be slower,
                    //   but it would provide the option to save/load _exactly_ as if you'd never quit at all,
                    //   with full undo/redo history/position/etc.!
                    if (save_mode) q(save_project{file_path});
                    else q(open_project{file_path});
                } else if (extension == FaustDspFileExtension) {
                    if (save_mode) q(save_faust_file{file_path});
                    else q(open_faust_file{file_path});
                }
            }
            q(close_file_dialog{});
        }
    } else {
        file_dialog->Close();
    }
}

// TODO handle errors
string File::read(const fs::path &path) {
    std::ifstream f(path, std::ios::in | std::ios::binary);
    const auto size = fs::file_size(path);
    string result(size, '\0');
    f.read(result.data(), long(size));
    return result;
}

// TODO handle errors
bool File::write(const fs::path &path, const string &contents) {
    std::fstream out_file;
    out_file.open(path, std::ios::out);
    if (out_file) {
        out_file << contents.c_str();
        out_file.close();
        return true;
    }

    return false;
}

bool File::write(const fs::path &path, const MessagePackBytes &contents) {
    std::fstream out_file(path, std::ios::out | std::ios::binary);
    if (out_file) {
        out_file.write(reinterpret_cast<const char *>(contents.data()), std::streamsize(contents.size()));
        out_file.close();
        return true;
    }

    return false;
}

void State::draw() const {
    DrawWindow(audio.settings);
    DrawWindow(audio.faust.editor, ImGuiWindowFlags_MenuBar);
    DrawWindow(audio.faust.log);
    DrawWindow(memory_editor, ImGuiWindowFlags_NoScrollbar);
    DrawWindow(state_viewer, ImGuiWindowFlags_MenuBar);
    DrawWindow(path_update_frequency, ImGuiWindowFlags_None);
    DrawWindow(demo, ImGuiWindowFlags_MenuBar);
    DrawWindow(metrics);
    DrawWindow(style);
    DrawWindow(tools);
    file.dialog.draw();
}
