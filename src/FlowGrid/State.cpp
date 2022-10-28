#include "App.h"
#include "StateJson.h"

#include <fstream>
#include <range/v3/view/concat.hpp>

#include "ImGuiFileDialog.h"
#include "imgui_memory_editor.h"

#include "FileDialog/ImGuiFileDialogDemo.h"
#include "Helper/File.h"

using namespace ImGui;
using namespace fg;
using namespace action;

//-----------------------------------------------------------------------------
// [SECTION] Actions
//-----------------------------------------------------------------------------

/**
 Provided actions are assumed to be chronologically consecutive.

 Cases:
 * `b` can be merged into `a`: return the merged action
 * `b` cancels out `a` (e.g. two consecutive boolean toggles on the same value): return `true`
 * `b` cannot be merged into `a`: return `false`

 Only handling cases where merges can be determined from two consecutive actions.
 One could imagine cases where an idempotent cycle could be determined only from > 2 actions.
 For example, incrementing modulo N would require N consecutive increments to determine that they could all be cancelled out.
*/
std::variant<Action, bool> merge(const Action &a, const Action &b) {
    const ID a_id = get_id(a);
    const ID b_id = get_id(b);

    switch (a_id) {
        case id<undo>:if (b_id == id<set_history_index>) return b;
            return b_id == id<redo>;
        case id<redo>:if (b_id == id<set_history_index>) return b;
            return b_id == id<undo>;
        case id<set_history_index>:
        case id<open_empty_project>:
        case id<open_default_project>:
        case id<show_open_project_dialog>:
        case id<open_file_dialog>:
        case id<close_file_dialog>:
        case id<show_save_project_dialog>:
        case id<close_application>:
        case id<set_imgui_settings>:
        case id<set_imgui_color_style>:
        case id<set_implot_color_style>:
        case id<set_flowgrid_color_style>:
        case id<set_flowgrid_diagram_color_style>:
        case id<set_flowgrid_diagram_layout_style>:
        case id<show_open_faust_file_dialog>:
        case id<show_save_faust_file_dialog>:if (a_id == b_id) return b;
            return false;
        case id<open_project>:
        case id<open_faust_file>:
        case id<save_faust_file>:if (a_id == b_id && json(a) == json(b)) return a;
            return false;
        case id<set_value>:if (a_id == b_id && std::get<set_value>(a).path == std::get<set_value>(b).path) return b;
            return false;
        case id<set_values>:if (a_id == b_id) return set_values{views::concat(std::get<set_values>(a).values, std::get<set_values>(b).values) | to<std::map>};
            return false;
        case id<toggle_value>:return a_id == b_id && std::get<toggle_value>(a).path == std::get<toggle_value>(b).path;
        default: return false;
    }
}

Gesture action::merge_gesture(const Gesture &gesture) {
    Gesture compressed_gesture;

    std::optional<const Action> active_action;
    for (size_t i = 0; i < gesture.size(); i++) {
        if (!active_action.has_value()) active_action.emplace(gesture[i]);
        const auto &a = active_action.value();
        const auto &b = gesture[i + 1];
        const auto merged = merge(a, b);
        std::visit(visitor{
            [&](const bool result) {
                if (result) i++; // The two actions in consideration (`a` and `b`) cancel out, so we add neither. (Skip over `b` entirely.)
                else compressed_gesture.emplace_back(a); // The left-side action (`a`) can't be merged into any further - nothing more we can do for it!
                active_action.reset(); // No merge in either case. Move on to try compressing the next action.
            },
            [&](const Action &result) {
                active_action.emplace(result); // `Action` result is a merged action. Don't add it yet - maybe we can merge more actions into it.
            },
        }, merged);
    }
    if (active_action.has_value()) compressed_gesture.emplace_back(active_action.value());

    return compressed_gesture;
}

//-----------------------------------------------------------------------------
// [SECTION] Fields
//-----------------------------------------------------------------------------

// Helper to display a (?) mark which shows a tooltip when hovered. From `imgui_demo.cpp`.
void StateMember::HelpMarker(const bool after) const {
    if (Help.empty()) return;

    if (after) SameLine();
    ::HelpMarker(Help.c_str());
    if (!after) SameLine();
}

bool Field::Bool::Draw() const {
    bool value = *this;
    const bool edited = Checkbox(Name.c_str(), &value);
    if (edited) q(toggle_value{Path});
    HelpMarker();
    return edited;
}
bool Field::Bool::DrawMenu() const {
    const bool value = *this;
    HelpMarker(false);
    const bool edited = MenuItem(Name.c_str(), nullptr, value);
    if (edited) q(toggle_value{Path});
    return edited;
}

bool Field::Int::Draw() const {
    int value = *this;
    const bool edited = SliderInt(Name.c_str(), &value, min, max, "%d", ImGuiSliderFlags_None);
    gestured();
    if (edited) q(set_value{Path, value});
    HelpMarker();
    return edited;
}
bool Field::Int::Draw(const vector<int> &options) const {
    bool edited = false;
    const int value = *this;
    if (BeginCombo(Name.c_str(), std::to_string(value).c_str())) {
        for (const auto option: options) {
            const bool is_selected = option == value;
            if (Selectable(std::to_string(option).c_str(), is_selected)) {
                q(set_value{Path, option});
                edited = true;
            }
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
    return edited;
}

bool Field::Float::Draw(ImGuiSliderFlags flags) const {
    float value = *this;
    const bool edited = SliderFloat(Name.c_str(), &value, min, max, fmt, flags);
    gestured();
    if (edited) q(set_value{Path, value});
    HelpMarker();
    return edited;
}

bool Field::Float::Draw(float v_speed, ImGuiSliderFlags flags) const {
    float value = *this;
    const bool edited = DragFloat(Name.c_str(), &value, v_speed, min, max, fmt, flags);
    gestured();
    if (edited) q(set_value{Path, value});
    HelpMarker();
    return edited;
}
bool Field::Float::Draw() const { return Draw(ImGuiSliderFlags_None); }

bool Field::Vec2::Draw(ImGuiSliderFlags flags) const {
    ImVec2 value = *this;
    const bool edited = SliderFloat2(Name.c_str(), (float *) &value, min, max, fmt, flags);
    gestured();
    if (edited) q(set_value{Path, value});
    HelpMarker();
    return edited;
}

bool Field::Vec2::Draw() const { return Draw(ImGuiSliderFlags_None); }

bool Field::Enum::Draw() const {
    return Draw(views::ints(0, int(names.size())) | to<vector<int>>); // todo if I stick with this pattern, cache.
}
bool Field::Enum::Draw(const vector<int> &choices) const {
    const int value = *this;
    bool edited = false;
    if (BeginCombo(Name.c_str(), names[value].c_str())) {
        for (int choice: choices) {
            const bool is_selected = choice == value;
            const auto &name = names[choice];
            if (Selectable(name.c_str(), is_selected)) {
                q(set_value{Path, choice});
                edited = true;
            }
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
    return edited;

}
bool Field::Enum::DrawMenu() const {
    const int value = *this;
    HelpMarker(false);
    bool edited = false;
    if (BeginMenu(Name.c_str())) {
        for (int i = 0; i < int(names.size()); i++) {
            const bool is_selected = value == i;
            if (MenuItem(names[i].c_str(), nullptr, is_selected)) {
                q(set_value{Path, i});
                edited = true;
            }
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
    return edited;
}

bool Field::Flags::Draw() const {
    const int value = *this;
    bool edited = false;
    if (TreeNodeEx(Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < int(items.size()); i++) {
            const auto &item = items[i];
            const int option_mask = 1 << i;
            bool is_selected = option_mask & value;
            if (Checkbox(item.Name.c_str(), &is_selected)) {
                q(set_value{Path, value ^ option_mask}); // toggle bit
                edited = true;
            }
            if (!item.Help.empty()) {
                SameLine();
                ::HelpMarker(item.Help.c_str());
            }
        }
        TreePop();
    }
    HelpMarker();
    return edited;
}
bool Field::Flags::DrawMenu() const {
    const int value = *this;
    HelpMarker(false);
    bool edited = false;
    if (BeginMenu(Name.c_str())) {
        for (int i = 0; i < int(items.size()); i++) {
            const auto &item = items[i];
            const int option_mask = 1 << i;
            const bool is_selected = option_mask & value;
            if (!item.Help.empty()) {
                ::HelpMarker(item.Help.c_str());
                SameLine();
            }
            if (MenuItem(item.Name.c_str(), nullptr, is_selected)) {
                q(set_value{Path, value ^ option_mask}); // toggle bit
                edited = true;
            }
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
    return edited;
}

bool Field::String::Draw() const {
    const string &value = *this;
    TextUnformatted(value.c_str());
    return false;
}
bool Field::String::Draw(const vector<string> &options) const {
    const string &value = *this;
    bool edited = false;
    if (BeginCombo(Name.c_str(), value.c_str())) {
        for (const auto &option: options) {
            const bool is_selected = option == value;
            if (Selectable(option.c_str(), is_selected)) {
                q(set_value{Path, option});
                edited = true;
            };
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
    return edited;
}

bool Color::Draw(ImGuiColorEditFlags flags, bool allow_auto) const {
    if (allow_auto) {
        // todo generalize auto colors (linked to ImGui colors) and use in FG colors
        auto temp = ImPlot::GetStyleColorVec4(index);
        const bool is_auto = ImPlot::IsColorAuto(index);
        if (!is_auto) PushStyleVar(ImGuiStyleVar_Alpha, 0.25);
        if (Button("Auto")) q(set_value{Path, is_auto ? temp : IMPLOT_AUTO_COL});
        if (!is_auto) PopStyleVar();
        SameLine();
    }
    ImVec4 value = *this;
    const bool edited = ImGui::ColorEdit4(path_label(Path).c_str(), (float *) &value, flags | (allow_auto ? ImGuiColorEditFlags_AlphaPreviewHalf : 0));
    gestured();
    if (edited) q(set_value{Path, value});
    return edited;
}

bool Color::Draw() const { return Draw(ImGuiColorEditFlags_None); }

//-----------------------------------------------------------------------------
// [SECTION] Helpers
//-----------------------------------------------------------------------------

ImRect RowItemRect() {
    const ImVec2 row_min = {GetWindowPos().x, GetCursorScreenPos().y};
    return {row_min, row_min + ImVec2{GetWindowWidth(), GetFontSize()}};
}

ImRect RowItemRatioRect(float ratio) {
    const ImVec2 row_min = {GetWindowPos().x, GetCursorScreenPos().y};
    return {row_min, row_min + ImVec2{GetWindowWidth() * std::clamp(ratio, 0.0f, 1.0f), GetFontSize()}};
}

void FillRowItemBg(const ImVec4 &col = s.Style.ImGui.Colors[ImGuiCol_FrameBgActive]) {
    const auto &rect = RowItemRect();
    GetWindowDrawList()->AddRectFilled(rect.Min, rect.Max, ImColor(col));
}

//-----------------------------------------------------------------------------
// [SECTION] Widgets
//-----------------------------------------------------------------------------

void fg::gestured() {
    if (ImGui::IsItemActivated()) c.is_widget_gesturing = true;
    if (ImGui::IsItemDeactivated()) c.is_widget_gesturing = false;
}

void fg::HelpMarker(const char *help) {
    TextDisabled("(?)");
    if (IsItemHovered()) {
        BeginTooltip();
        PushTextWrapPos(GetFontSize() * 35);
        TextUnformatted(help);
        PopTextWrapPos();
        EndTooltip();
    }
}

void fg::MenuItem(ActionID action_id) {
    const char *menu_label = action::get_menu_label(action_id);
    const char *shortcut = action::shortcut_for_id.contains(action_id) ? action::shortcut_for_id.at(action_id).c_str() : nullptr;
    if (ImGui::MenuItem(menu_label, shortcut, false, c.action_allowed(action_id))) q(action::create(action_id));
}

bool fg::JsonTreeNode(const string &label, JsonTreeNodeFlags flags, const char *id) {
    const bool highlighted = flags & JsonTreeNodeFlags_Highlighted;
    const bool disabled = flags & JsonTreeNodeFlags_Disabled;
    const ImGuiTreeNodeFlags imgui_flags = flags & JsonTreeNodeFlags_DefaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;

    if (disabled) ImGui::BeginDisabled();
    if (highlighted) ImGui::PushStyleColor(ImGuiCol_Text, s.Style.FlowGrid.Colors[FlowGridCol_HighlightText]);
    const bool is_open = id ? ImGui::TreeNodeEx(id, imgui_flags, "%s", label.c_str()) : ImGui::TreeNodeEx(label.c_str(), imgui_flags);
    if (highlighted) ImGui::PopStyleColor();
    if (disabled) ImGui::EndDisabled();

    return is_open;
}

void fg::JsonTree(const string &label, const json &value, JsonTreeNodeFlags node_flags, const char *id) {
    if (value.is_null()) {
        ImGui::TextUnformatted(label.empty() ? "(null)" : label.c_str());
    } else if (value.is_object()) {
        if (label.empty() || JsonTreeNode(label, node_flags, id)) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                JsonTree(it.key(), it.value(), node_flags);
            }
            if (!label.empty()) ImGui::TreePop();
        }
    } else if (value.is_array()) {
        if (label.empty() || JsonTreeNode(label, node_flags, id)) {
            int i = 0;
            for (const auto &it: value) {
                JsonTree(std::to_string(i), it, node_flags);
                i++;
            }
            if (!label.empty()) ImGui::TreePop();
        }
    } else {
        if (label.empty()) ImGui::TextUnformatted(value.dump().c_str());
        else ImGui::Text("%s: %s", label.c_str(), value.dump().c_str());
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Window methods
//-----------------------------------------------------------------------------

void Window::DrawWindow(ImGuiWindowFlags flags) const {
    if (!Visible) return;

    bool open = Visible;
    if (Begin(Name.c_str(), &open, flags)) {
        if (open) Draw();
    }
    End();

    if (Visible && !open) q(set_value{Visible.Path, false});
}

void Window::Dock(ImGuiID node_id) const {
    DockBuilderDockWindow(Name.c_str(), node_id);
}

bool Window::ToggleMenuItem() const {
    const bool edited = MenuItem(Name.c_str(), nullptr, Visible);
    if (edited) q(toggle_value{Visible.Path});
    return edited;
}

void Window::SelectTab() const {
    FindImGuiWindow().DockNode->SelectedTabId = FindImGuiWindow().TabId;
}

void Process::Draw() const {
    Running.Draw();
}

void Info::Draw() const {
    const auto hovered_id = GetHoveredID();
    if (hovered_id && StateMember::WithID.contains(hovered_id)) {
        const auto *member = StateMember::WithID.at(hovered_id);
        const string &help = member->Help;
        PushTextWrapPos(0);
        TextUnformatted((help.empty() ? format("No info available for {}.", member->Name) : help).c_str());
    }
}

static int PrevFontIndex = 0;
static float PrevFontScale = 1.0;

void State::Draw() const {
    if (PrevFontIndex != Style.ImGui.FontIndex) {
        GetIO().FontDefault = GetIO().Fonts->Fonts[Style.ImGui.FontIndex];
        PrevFontIndex = Style.ImGui.FontIndex;
    }
    if (PrevFontScale != Style.ImGui.FontScale) {
        GetIO().FontGlobalScale = Style.ImGui.FontScale / Style::ImGuiStyle::FontAtlasScale;
        PrevFontScale = Style.ImGui.FontScale;
    }

    if (BeginMainMenuBar()) {
        if (BeginMenu("File")) {
            MenuItem(action::id<open_empty_project>);
            MenuItem(action::id<show_open_project_dialog>);

            const auto &recently_opened_paths = c.preferences.recently_opened_paths;
            if (BeginMenu("Open recent project", !recently_opened_paths.empty())) {
                for (const auto &recently_opened_path: recently_opened_paths) {
                    if (MenuItem(recently_opened_path.filename().c_str())) q(open_project{recently_opened_path});
                }
                EndMenu();
            }

            MenuItem(action::id<save_current_project>);
            MenuItem(action::id<show_save_project_dialog>);
            MenuItem(action::id<open_default_project>);
            MenuItem(action::id<save_default_project>);
            EndMenu();
        }
        if (BeginMenu("Edit")) {
            MenuItem(action::id<undo>);
            MenuItem(action::id<redo>);
            EndMenu();
        }
        if (BeginMenu("Windows")) {
            if (BeginMenu("Debug")) {
                DebugLog.ToggleMenuItem();
                StackTool.ToggleMenuItem();
                StateViewer.ToggleMenuItem();
                PathUpdateFrequency.ToggleMenuItem();
                StateMemoryEditor.ToggleMenuItem();
                ProjectPreview.ToggleMenuItem();
                EndMenu();
            }
            if (BeginMenu("Audio")) {
                Audio.ToggleMenuItem();
                if (BeginMenu("Faust")) {
                    Audio.Faust.Editor.ToggleMenuItem();
                    Audio.Faust.Diagram.ToggleMenuItem();
                    Audio.Faust.Params.ToggleMenuItem();
                    Audio.Faust.Log.ToggleMenuItem();
                    EndMenu();
                }
                EndMenu();
            }
            Metrics.ToggleMenuItem();
            Style.ToggleMenuItem();
            Demo.ToggleMenuItem();
            EndMenu();
        }
        EndMainMenuBar();
    }

    // Good initial layout setup example in this issue: https://github.com/ocornut/imgui/issues/3548
    auto dockspace_id = DockSpaceOverViewport(nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    int frame_count = GetCurrentContext()->FrameCount;
    if (frame_count == 1) {
        auto faust_editor_node_id = dockspace_id;
        auto sidebar_node_id = DockBuilderSplitNode(faust_editor_node_id, ImGuiDir_Right, 0.15f, nullptr, &faust_editor_node_id);
        auto settings_node_id = DockBuilderSplitNode(faust_editor_node_id, ImGuiDir_Left, 0.3f, nullptr, &faust_editor_node_id);
        auto utilities_node_id = DockBuilderSplitNode(settings_node_id, ImGuiDir_Down, 0.5f, nullptr, &settings_node_id);
        auto debug_node_id = DockBuilderSplitNode(faust_editor_node_id, ImGuiDir_Down, 0.3f, nullptr, &faust_editor_node_id);
        auto faust_tools_node_id = DockBuilderSplitNode(faust_editor_node_id, ImGuiDir_Down, 0.5f, nullptr, &faust_editor_node_id);

        ApplicationSettings.Dock(settings_node_id);
        Audio.Dock(settings_node_id);

        Audio.Faust.Editor.Dock(faust_editor_node_id);
        Audio.Faust.Diagram.Dock(faust_tools_node_id);
        Audio.Faust.Params.Dock(faust_tools_node_id);

        DebugLog.Dock(debug_node_id);
        StackTool.Dock(debug_node_id);
        Audio.Faust.Log.Dock(debug_node_id);
        StateViewer.Dock(debug_node_id);
        StateMemoryEditor.Dock(debug_node_id);
        PathUpdateFrequency.Dock(debug_node_id);
        ProjectPreview.Dock(debug_node_id);

        Metrics.Dock(utilities_node_id);
        Style.Dock(utilities_node_id);
        Demo.Dock(utilities_node_id);

        Info.Dock(sidebar_node_id);
    } else if (frame_count == 2) {
        // Doesn't work on the first draw: https://github.com/ocornut/imgui/issues/2304
        DebugLog.SelectTab(); // not visible by default anymore
        Metrics.SelectTab();
    }

    ApplicationSettings.DrawWindow();
    Audio.DrawWindow();

    Audio.Faust.Editor.DrawWindow(ImGuiWindowFlags_MenuBar);
    Audio.Faust.Diagram.DrawWindow(ImGuiWindowFlags_MenuBar);
    Audio.Faust.Params.DrawWindow();
    Audio.Faust.Log.DrawWindow();

    DebugLog.DrawWindow();
    StackTool.DrawWindow();
    StateViewer.DrawWindow(ImGuiWindowFlags_MenuBar);
    PathUpdateFrequency.DrawWindow();
    StateMemoryEditor.DrawWindow(ImGuiWindowFlags_NoScrollbar);
    ProjectPreview.DrawWindow();

    Metrics.DrawWindow();
    Style.DrawWindow();
    Demo.DrawWindow(ImGuiWindowFlags_MenuBar);
    FileDialog.Draw();
    Info.DrawWindow();
}

ImGuiSettingsData::ImGuiSettingsData(ImGuiContext *ctx) {
    SaveIniSettingsToMemory(); // Populates the `Settings` context members
    Nodes = ctx->DockContext.NodesSettings; // already an ImVector
    // Convert `ImChunkStream` to `ImVector`.
    for (auto *ws = ctx->SettingsWindows.begin(); ws != nullptr; ws = ctx->SettingsWindows.next_chunk(ws)) {
        Windows.push_back(*ws);
    }
    for (auto *ts = ctx->SettingsTables.begin(); ts != nullptr; ts = ctx->SettingsTables.next_chunk(ts)) {
        ImGuiTableColumnSettings *column_settings = ts->GetColumnSettings();
        const auto *table = TableFindByID(ts->ID);
        ImGuiTableColumn *column = table->Columns.Data;
        vector<TableColumnSettings> cs;
        for (int n = 0; n < ts->ColumnsCount; n++, column++, column_settings++) cs.emplace_back(*column_settings);
        Tables.push_back({*ts, cs});
    }
}

// Copied from `imgui.cpp::ApplyWindowSettings`
static void ApplyWindowSettings(ImGuiWindow *window, ImGuiWindowSettings *settings) {
    if (!window) return; // TODO log

    const ImGuiViewport *main_viewport = GetMainViewport();
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

// Adapted from `imgui_tables.cpp::TableLoadSettings`
static void ApplyTableSettings(ImGuiTable *table, const TableSettings &settings) {
    if (!table) return; // todo log

    table->IsSettingsRequestLoad = false; // todo remove this var/behavior?
    table->SettingsLoadedFlags = settings.Table.SaveFlags; // todo remove this var/behavior?
    table->RefScale = settings.Table.RefScale;

    // Serialize ImGuiTableSettings/ImGuiTableColumnSettings into ImGuiTable/ImGuiTableColumn
    ImU64 display_order_mask = 0;
    for (const auto &column_settings: settings.Columns) {
        int column_n = column_settings.Index;
        if (column_n < 0 || column_n >= table->ColumnsCount)
            continue;

        ImGuiTableColumn *column = &table->Columns[column_n];
        if (settings.Table.SaveFlags & ImGuiTableFlags_Resizable) {
            if (column_settings.IsStretch) column->StretchWeight = column_settings.WidthOrWeight;
            else column->WidthRequest = column_settings.WidthOrWeight;
            column->AutoFitQueue = 0x00;
        }
        column->DisplayOrder = settings.Table.SaveFlags & ImGuiTableFlags_Reorderable ? column_settings.DisplayOrder : (ImGuiTableColumnIdx) column_n;
        display_order_mask |= (ImU64) 1 << column->DisplayOrder;
        column->IsUserEnabled = column->IsUserEnabledNextFrame = column_settings.IsEnabled;
        column->SortOrder = column_settings.SortOrder;
        column->SortDirection = column_settings.SortDirection;
    }

    // Validate and fix invalid display order data
    const ImU64 expected_display_order_mask = settings.Table.ColumnsCount == 64 ? ~0 : ((ImU64) 1 << settings.Table.ColumnsCount) - 1;
    if (display_order_mask != expected_display_order_mask)
        for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
            table->Columns[column_n].DisplayOrder = (ImGuiTableColumnIdx) column_n;

    // Rebuild index
    for (int column_n = 0; column_n < table->ColumnsCount; column_n++)
        table->DisplayOrderToIndex[table->Columns[column_n].DisplayOrder] = (ImGuiTableColumnIdx) column_n;
}

void Style::ImGuiStyle::Apply(ImGuiContext *ctx) const {
    auto &style = ctx->Style;
    style.Alpha = Alpha;
    style.DisabledAlpha = DisabledAlpha;
    style.WindowPadding = WindowPadding;
    style.WindowRounding = WindowRounding;
    style.WindowBorderSize = WindowBorderSize;
    style.WindowMinSize = WindowMinSize;
    style.WindowTitleAlign = WindowTitleAlign;
    style.WindowMenuButtonPosition = WindowMenuButtonPosition;
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
    style.ColorButtonPosition = ColorButtonPosition;
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
    for (int i = 0; i < ImGuiCol_COUNT; i++) style.Colors[i] = Colors[i];
}

void Style::ImPlotStyle::Apply(ImPlotContext *ctx) const {
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
    style.Colormap = Colormap;
    style.UseLocalTime = UseLocalTime;
    style.UseISO8601 = UseISO8601;
    style.Use24HourClock = Use24HourClock;
    for (int i = 0; i < ImPlotCol_COUNT; i++) style.Colors[i] = Colors[i];
    ImPlot::BustItemCache();
}

void ImGuiSettings::Apply(ImGuiContext *ctx) const {
    // Clear
    DockSettingsHandler_ClearAll(ctx, nullptr);

    // Apply
    for (auto ws: Windows) ApplyWindowSettings(FindWindowByID(ws.ID), &ws);
    for (auto &ts: Tables) ApplyTableSettings(TableFindByID(ts.Table.ID), ts);

    ctx->DockContext.NodesSettings = Nodes; // already an ImVector
    DockSettingsHandler_ApplyAll(ctx, nullptr);

    // Other housekeeping to emulate `LoadIniSettingsFromMemory`
    ctx->SettingsLoaded = true;
    ctx->SettingsDirty = false;
}

//-----------------------------------------------------------------------------
// [SECTION] State windows
//-----------------------------------------------------------------------------

// TODO option to indicate relative update-recency
void StateViewer::StateJsonTree(const string &key, const json &value, const JsonPath &path) const {
    const auto path_string = path.to_string();
    const string &leaf_name = path == RootPath ? path_string : path.back();
    const auto &parent_path = path == RootPath ? path : path.parent_pointer();
    const bool is_array_item = is_integer(leaf_name);
    const int array_index = is_array_item ? std::stoi(leaf_name) : -1;
    const bool is_imgui_color = parent_path == s.Style.ImGui.Colors.Path;
    const bool is_implot_color = parent_path == s.Style.ImPlot.Colors.Path;
    const bool is_flowgrid_color = parent_path == s.Style.FlowGrid.Colors.Path;
    const auto &label = LabelMode == Annotated ?
                        (is_imgui_color ? s.Style.ImGui.Colors[array_index].Name :
                         is_implot_color ? s.Style.ImPlot.Colors[array_index].Name :
                         is_flowgrid_color ? s.Style.FlowGrid.Colors[array_index].Name :
                         is_array_item ? leaf_name : key) : key;
    if (AutoSelect) {
        const auto &update_paths = c.state_stats.latest_updated_paths;
        const auto is_ancestor_path = [path_string](const string &candidate_path) { return candidate_path.rfind(path_string, 0) == 0; };
        const bool was_recently_updated = std::find_if(update_paths.begin(), update_paths.end(), is_ancestor_path) != update_paths.end();
        SetNextItemOpen(was_recently_updated);
        if (was_recently_updated) FillRowItemBg(s.Style.ImGui.Colors[ImGuiCol_FrameBg]);
    }

    // Flash background color of nodes when its corresponding path updates.
    if (c.state_stats.latest_update_time_for_path.contains(path)) {
        const auto latest_update_time = c.state_stats.latest_update_time_for_path.contains(path) ? c.state_stats.latest_update_time_for_path.at(path) : TimePoint{};
        const float flash_elapsed_ratio = fsec(Clock::now() - latest_update_time).count() / s.Style.FlowGrid.FlashDurationSec;
        ImVec4 flash_color = s.Style.FlowGrid.Colors[FlowGridCol_GestureIndicator];
        flash_color.w = max(0.0f, 1 - flash_elapsed_ratio);
        FillRowItemBg(flash_color);
    }

    JsonTreeNodeFlags flags = JsonTreeNodeFlags_None;
    if (LabelMode == Annotated && (is_imgui_color || is_implot_color || is_flowgrid_color)) flags |= JsonTreeNodeFlags_Highlighted;
    if (AutoSelect) flags |= JsonTreeNodeFlags_Disabled;

    // The rest below is structurally identical to `fg::Widgets::JsonTree`.
    // Couldn't find an easy/clean way to inject the above into each recursive call.
    if (value.is_null()) {
        TextUnformatted(label.c_str());
    } else if (value.is_object()) {
        if (JsonTreeNode(label, flags)) {
            for (auto it = value.begin(); it != value.end(); ++it) {
                StateJsonTree(it.key(), it.value(), path / it.key());
            }
            TreePop();
        }
    } else if (value.is_array()) {
        if (JsonTreeNode(label, flags)) {
            int i = 0;
            for (const auto &it: value) {
                StateJsonTree(std::to_string(i), it, path / std::to_string(i));
                i++;
            }
            TreePop();
        }
    } else {
        Text("%s: %s", label.c_str(), value.dump().c_str());
    }
}

void StateViewer::Draw() const {
    if (BeginMenuBar()) {
        if (BeginMenu("Settings")) {
            AutoSelect.DrawMenu();
            LabelMode.DrawMenu();
            EndMenu();
        }
        EndMenuBar();
    }

    StateJsonTree("State", c.get_project_json());
}

void StateMemoryEditor::Draw() const {
    static MemoryEditor memory_editor;
    static bool first_render{true};
    if (first_render) {
        memory_editor.OptShowDataPreview = true;
//        memory_editor.WriteFn = ...; todo write_state_bytes action
        first_render = false;
    }

    const void *mem_data{&s};
    memory_editor.DrawContents(mem_data, sizeof(s));
}

void StatePathUpdateFrequency::Draw() const {
    if (c.state_stats.committed_update_times_for_path.empty() && c.state_stats.gesture_update_times_for_path.empty()) {
        Text("No state updates yet.");
        return;
    }

    auto &[labels, values] = c.state_stats.PathUpdateFrequency;
    if (ImPlot::BeginPlot("Path update frequency", {-1, float(labels.size()) * 30 + 60}, ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Number of updates", nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert);

        // Hack to allow `SetupAxisTicks` without breaking on assert `n_ticks > 1`: Just add an empty label and only plot one value.
        // todo fix in ImPlot
        if (labels.size() == 1) labels.emplace_back("");

        // todo add an axis flag to exclude non-integer ticks
        // todo add an axis flag to show last tick
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, double(labels.size() - 1), int(labels.size()), labels.data(), false);
        static const char *item_labels[] = {"Committed updates", "Active updates"};
        const bool has_gesture = !c.state_stats.gesture_update_times_for_path.empty();
        const int item_count = has_gesture ? 2 : 1;
        const int group_count = has_gesture ? int(values.size()) / 2 : int(values.size());
        ImPlot::PlotBarGroups(item_labels, values.data(), item_count, group_count, 0.75, 0, ImPlotBarGroupsFlags_Horizontal | ImPlotBarGroupsFlags_Stacked);

        ImPlot::EndPlot();
    }
}

void ProjectPreview::Draw() const {
    Format.Draw();
    Raw.Draw();

    Separator();

    const json project_json = c.get_project_json(ProjectFormat(int(Format)));
    if (Raw) TextUnformatted(project_json.dump(4).c_str());
    else JsonTree("", project_json, JsonTreeNodeFlags_DefaultOpen);
}

//-----------------------------------------------------------------------------
// [SECTION] Style editors
//-----------------------------------------------------------------------------

bool Colors::Draw() const {
    bool changed = false;
    if (BeginTabItem(Name.c_str(), nullptr, ImGuiTabItemFlags_NoPushId)) {
        static ImGuiTextFilter filter;
        filter.Draw("Filter colors", GetFontSize() * 16);

        static ImGuiColorEditFlags alpha_flags = 0;
        if (RadioButton("Opaque", alpha_flags == ImGuiColorEditFlags_None)) alpha_flags = ImGuiColorEditFlags_None;
        SameLine();
        if (RadioButton("Alpha", alpha_flags == ImGuiColorEditFlags_AlphaPreview)) alpha_flags = ImGuiColorEditFlags_AlphaPreview;
        SameLine();
        if (RadioButton("Both", alpha_flags == ImGuiColorEditFlags_AlphaPreviewHalf)) alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf;
        SameLine();
        ::HelpMarker("In the color list:\n"
                     "Left-click on color square to open color picker.\n"
                     "Right-click to open edit options menu.");

        BeginChild("##colors", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NavFlattened);
        PushItemWidth(-160);

        const auto &style = GetStyle();
        for (int i = 0; i < int(size()); i++) {
            const char *name = items[i].Name.c_str();
            if (!filter.PassFilter(name)) continue;

            PushID(i);
            changed |= items[i].Draw(ImGuiColorEditFlags_AlphaBar | alpha_flags, allow_auto);
            SameLine(0, style.ItemInnerSpacing.x);
            TextUnformatted(name);
            PopID();
        }
        if (allow_auto) {
            Separator();
            PushTextWrapPos(0);
            Text("Colors that are set to Auto will be automatically deduced from your ImGui style or the current ImPlot colormap.\n"
                 "If you want to style individual plot items, use Push/PopStyleColor around its function.");
            PopTextWrapPos();
        }

        PopItemWidth();
        EndChild();
        EndTabItem();
    }
    return changed;
}

// Returns `true` if style changes.
void Style::ImGuiStyle::Draw() const {
    static int style_idx = -1;
    if (Combo("Colors##Selector", &style_idx, "Dark\0Light\0Classic\0")) q(set_imgui_color_style{style_idx});

    const auto &io = GetIO();
    const auto *font_current = GetFont();
    if (BeginCombo("Fonts", font_current->GetDebugName())) {
        for (int n = 0; n < io.Fonts->Fonts.Size; n++) {
            const auto *font = io.Fonts->Fonts[n];
            PushID(font);
            if (Selectable(font->GetDebugName(), font == font_current)) q(set_value{FontIndex.Path, n});
            PopID();
        }
        ImGui::EndCombo();
    }

    // Simplified Settings (expose floating-pointer border sizes as boolean representing 0 or 1)
    {
        bool border = WindowBorderSize > 0;
        if (Checkbox("WindowBorder", &border)) q(set_value{WindowBorderSize.Path, border ? 1 : 0});
    }
    SameLine();
    {
        bool border = FrameBorderSize > 0;
        if (Checkbox("FrameBorder", &border)) q(set_value{FrameBorderSize.Path, border ? 1 : 0});
    }
    SameLine();
    {
        bool border = PopupBorderSize > 0;
        if (Checkbox("PopupBorder", &border)) q(set_value{PopupBorderSize.Path, border ? 1 : 0});
    }

    Separator();

    if (BeginTabBar("", ImGuiTabBarFlags_None)) {
        if (BeginTabItem("Sizes", nullptr, ImGuiTabItemFlags_NoPushId)) {
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

        Colors.Draw();

        if (BeginTabItem("Fonts")) {
            ShowFontAtlas(io.Fonts);

            PushItemWidth(GetFontSize() * 8);
            FontScale.Draw(0.005f, ImGuiSliderFlags_None);
            PopItemWidth();

            EndTabItem();
        }

        if (BeginTabItem("Rendering", nullptr, ImGuiTabItemFlags_NoPushId)) {
            AntiAliasedLines.Draw();
            AntiAliasedLinesUseTex.Draw();
            AntiAliasedFill.Draw();
            PushItemWidth(GetFontSize() * 8);
            CurveTessellationTol.Draw(0.02f, ImGuiSliderFlags_None);

            // When editing the "Circle Segment Max Error" value, draw a preview of its effect on auto-tessellated circles.
            CircleTessellationMaxError.Draw(0.005f, ImGuiSliderFlags_AlwaysClamp);
            if (IsItemActive()) {
                SetNextWindowPos(GetCursorScreenPos());
                BeginTooltip();
                TextUnformatted("(R = radius, N = number of segments)");
                Spacing();
                ImDrawList *draw_list = GetWindowDrawList();
                const float min_widget_width = CalcTextSize("N: MMM\nR: MMM").x;
                for (int n = 0; n < 8; n++) {
                    const float RAD_MIN = 5;
                    const float RAD_MAX = 70;
                    const float rad = RAD_MIN + (RAD_MAX - RAD_MIN) * float(n) / 7.0f;

                    BeginGroup();

                    Text("R: %.f\nN: %d", rad, draw_list->_CalcCircleAutoSegmentCount(rad));

                    const float canvas_width = ImMax(min_widget_width, rad * 2);
                    const ImVec2 offset = {floorf(canvas_width * 0.5f), floorf(RAD_MAX)};
                    const ImVec2 p1 = GetCursorScreenPos();
                    draw_list->AddCircle(p1 + offset, rad, GetColorU32(ImGuiCol_Text));
                    Dummy(ImVec2(canvas_width, RAD_MAX * 2));

                    EndGroup();
                    SameLine();
                }
                EndTooltip();
            }
            SameLine();
            HelpMarker("When drawing circle primitives with \"num_segments == 0\" tesselation will be calculated automatically.");

            Alpha.Draw(0.005, ImGuiSliderFlags_None);
            DisabledAlpha.Draw(0.005, ImGuiSliderFlags_None);
            PopItemWidth();

            EndTabItem();
        }

        EndTabBar();
    }
}

void Style::ImPlotStyle::Draw() const {
    static int style_idx = -1;
    if (Combo("Colors##Selector", &style_idx, "Auto\0Dark\0Light\0Classic\0")) q(set_implot_color_style{style_idx});

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
        Colors.Draw();
        EndTabBar();
    }
}

void Style::FlowGridStyle::Draw() const {
    static int colors_idx = -1, diagram_colors_idx = -1, diagram_layout_idx = -1;
    if (Combo("Colors", &colors_idx, "Dark\0Light\0Classic\0")) q(set_flowgrid_color_style{colors_idx});
    if (Combo("Diagram colors", &diagram_colors_idx, "Dark\0Light\0Classic\0Faust\0")) q(set_flowgrid_diagram_color_style{diagram_colors_idx});
    if (Combo("Diagram layout", &diagram_layout_idx, "FlowGrid\0Faust\0")) q(set_flowgrid_diagram_layout_style{diagram_layout_idx});
    FlashDurationSec.Draw();

    if (BeginTabBar("")) {
        if (BeginTabItem("Faust diagram", nullptr, ImGuiTabItemFlags_NoPushId)) {
            DiagramFoldComplexity.Draw();
            const bool ScaleFill = DiagramScaleFill;
            DiagramScaleFill.Draw();
            if (ScaleFill) ImGui::BeginDisabled();
            const ImVec2 scale_before = DiagramScale;
            if (DiagramScale.Draw() && DiagramScaleLinked) {
                c.run_queued_actions();
                const ImVec2 scale_after = DiagramScale;
                q(set_value{DiagramScale.Path, scale_after.x != scale_before.x ?
                                               ImVec2{scale_after.x, scale_after.x} :
                                               ImVec2{scale_after.y, scale_after.y}});
                c.run_queued_actions();
            }
            if (DiagramScaleLinked.Draw() && !DiagramScaleLinked) {
                const ImVec2 scale = DiagramScale;
                const float min_scale = min(scale.x, scale.y);
                q(set_value{DiagramScale.Path, ImVec2{min_scale, min_scale}});
            }
            if (ScaleFill) {
                SameLine();
                Text("Uncheck 'ScaleFill' to edit scale settings.");
                ImGui::EndDisabled();
            }
            DiagramDirection.Draw();
            DiagramOrientationMark.Draw();
            if (DiagramOrientationMark) {
                SameLine();
                SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
                DiagramOrientationMarkRadius.Draw();
            }
            DiagramRouteFrame.Draw();
            DiagramSequentialConnectionZigzag.Draw();
            DiagramTopLevelMargin.Draw();
            DiagramDecorateMargin.Draw();
            DiagramDecorateLineWidth.Draw();
            DiagramDecorateCornerRadius.Draw();
            DiagramBoxCornerRadius.Draw();
            DiagramBinaryHorizontalGapRatio.Draw();
            DiagramWireGap.Draw();
            DiagramGap.Draw();
            DiagramWireWidth.Draw();
            DiagramArrowSize.Draw();
            DiagramInverterRadius.Draw();
            EndTabItem();
        }
        if (BeginTabItem("Faust params", nullptr, ImGuiTabItemFlags_NoPushId)) {
            ParamsHeaderTitles.Draw();
            ParamsMinHorizontalItemWidth.Draw();
            ParamsMaxHorizontalItemWidth.Draw();
            ParamsMinVerticalItemHeight.Draw();
            ParamsMinKnobItemSize.Draw();
            ParamsAlignmentHorizontal.Draw();
            ParamsAlignmentVertical.Draw();
            Spacing();
            ParamsWidthSizingPolicy.Draw();
            ParamsTableFlags.Draw();
            EndTabItem();
        }

        Colors.Draw();
        EndTabBar();
    }
}

void Style::Draw() const {
    if (BeginTabBar("")) {
        if (BeginTabItem(FlowGrid.Name.c_str())) {
            FlowGrid.Draw();
            EndTabItem();
        }
        if (BeginTabItem(ImGui.Name.c_str())) {
            ImGui.Draw();
            EndTabItem();
        }
        if (BeginTabItem(ImPlot.Name.c_str())) {
            ImPlot.Draw();
            EndTabItem();
        }
        EndTabBar();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Other windows
//-----------------------------------------------------------------------------

void ApplicationSettings::Draw() const {
    int value = c.state_history_index;
    if (SliderInt("History index", &value, 0, Context::history_size() - 1)) q(set_history_index{value});
    GestureDurationSec.Draw();
}

const vector<int> Audio::PrioritizedDefaultSampleRates = {48000, 44100, 96000};
const vector<Audio::IoFormat> Audio::PrioritizedDefaultFormats = {
    IoFormat_Float64NE,
    IoFormat_Float32NE,
    IoFormat_S32NE,
    IoFormat_S16NE,
    IoFormat_Invalid,
};

void Demo::ImGuiDemo::Draw() const {
    ShowDemoWindow();
}
void Demo::ImPlotDemo::Draw() const {
    ImPlot::ShowDemoWindow();
}
void Demo::FileDialogDemo::Draw() const {
    IGFD::ShowDemoWindow();
}
void Demo::Draw() const {
    if (BeginTabBar("")) {
        if (BeginTabItem(ImGui.Name.c_str())) {
            ImGui.Draw();
            EndTabItem();
        }
        if (BeginTabItem(ImPlot.Name.c_str())) {
            ImPlot.Draw();
            EndTabItem();
        }
        if (BeginTabItem(FileDialog.Name.c_str())) {
            FileDialog.Draw();
            EndTabItem();
        }
        EndTabBar();
    }
}

void ShowJsonPatchOpMetrics(const JsonPatchOp &patch_op) {
    BulletText("Path: %s", patch_op.path.to_string().c_str());
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

void ShowGesture(const Gesture &gesture) {
    for (size_t action_i = 0; action_i < gesture.size(); action_i++) {
        const auto &action = gesture[action_i];
        const auto &label = action::get_name(action);
        JsonTree(label, json(action)[1], JsonTreeNodeFlags_None, std::to_string(action_i).c_str());
    }
}

void Metrics::FlowGridMetrics::Draw() const {
    {
        // Gestures (semantically grouped lists of actions)

        // Active (uncompressed) gesture
        const bool widget_gesture = c.is_widget_gesturing;
        const bool active_gesture_present = !c.active_gesture.empty();
        if (active_gesture_present || widget_gesture) {
            // Gesture completion progress bar
            const auto row_item_ratio_rect = RowItemRatioRect(1 - c.gesture_time_remaining_sec / s.ApplicationSettings.GestureDurationSec);
            GetWindowDrawList()->AddRectFilled(row_item_ratio_rect.Min, row_item_ratio_rect.Max, ImColor(s.Style.FlowGrid.Colors[FlowGridCol_GestureIndicator]));

            const auto &active_gesture_title = string("Active gesture") + (active_gesture_present ? " (uncompressed)" : "");
            if (TreeNodeEx(active_gesture_title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (widget_gesture) FillRowItemBg();
                else BeginDisabled();
                Text("Widget gesture: %s", widget_gesture ? "true" : "false");
                if (!widget_gesture) EndDisabled();

                if (active_gesture_present) ShowGesture(c.active_gesture);
                else Text("No actions yet");
                TreePop();
            }
        } else {
            BeginDisabled();
            Text("No active gesture");
            EndDisabled();
        }

        // Committed gestures
        const bool has_gestures = !c.gestures.empty();
        if (!has_gestures) BeginDisabled();
        if (TreeNodeEx("Committed gestures", ImGuiTreeNodeFlags_DefaultOpen, "Committed gestures (%lu)", c.gestures.size())) {
            for (size_t gesture_i = 0; gesture_i < c.gestures.size(); gesture_i++) {
                if (TreeNodeEx(std::to_string(gesture_i).c_str(), gesture_i == c.gestures.size() - 1 ? ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None)) {
                    // todo link gesture actions and corresponding diff (note some action gestures won't have a diff, like `undo`)
                    const auto &gesture = c.gestures[gesture_i];
                    ShowGesture(gesture);
                    TreePop();
                }
            }
            TreePop();
        }
        if (!has_gestures) EndDisabled();
    }
    Separator();
    {
        // Diffs
        const bool has_diffs = Context::history_size() > 1;
        if (!has_diffs) BeginDisabled();
        if (TreeNodeEx("Diffs", ImGuiTreeNodeFlags_DefaultOpen, "Diffs (Count: %d, Current index: %d)", Context::history_size() - 1, c.state_history_index)) {
            for (int i = 0; i < Context::history_size() - 1; i++) {
                if (TreeNodeEx(std::to_string(i).c_str(), i == c.state_history_index - 1 ? (ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_DefaultOpen) : ImGuiTreeNodeFlags_None)) {
                    const auto &diff = Context::create_diff(i);
                    // todo link to gesture corresponding to diff
//                    if (diff.action_names.size() == 1) {
//                        BulletText("Action name: %s", (*diff.action_names.begin()).c_str());
//                    } else {
//                        if (TreeNodeEx("Action names", ImGuiTreeNodeFlags_DefaultOpen, "%lu actions", diff.action_names.size())) {
//                            for (const auto &action_name: diff.action_names) BulletText("%s", action_name.c_str());
//                            TreePop();
//                        }
//                    }
                    ShowJsonPatchMetrics(diff.Patch);

                    BulletText("Time: %s", format("{}\n", diff.Time).c_str());
                    TreePop();
                }
            }
            TreePop();
        }
        if (!has_diffs) EndDisabled();
    }
    Separator();
    {
        // Preferences
        const bool has_recently_opened_paths = !c.preferences.recently_opened_paths.empty();
        if (TreeNodeEx("Preferences", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (SmallButton("Clear")) c.clear_preferences();
            SameLine();
            ShowRelativePaths.Draw();

            if (!has_recently_opened_paths) BeginDisabled();
            if (TreeNodeEx("Recently opened paths", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto &recently_opened_path: c.preferences.recently_opened_paths) {
                    BulletText("%s", (ShowRelativePaths ? fs::relative(recently_opened_path) : recently_opened_path).c_str());
                }
                TreePop();
            }
            if (!has_recently_opened_paths) EndDisabled();

            TreePop();
        }
    }
    Separator();
    {
        // Various internals
        Text("Action variant size: %lu bytes", sizeof(Action));
        SameLine();
        HelpMarker("All actions are internally stored in an `std::variant`, which must be large enough to hold its largest type. "
                   "Thus, it's important to keep action data small.");
    }
}
void Metrics::ImGuiMetrics::Draw() const { ShowMetricsWindow(); }
void Metrics::ImPlotMetrics::Draw() const { ImPlot::ShowMetricsWindow(); }

void Metrics::Draw() const {
    if (BeginTabBar("")) {
        if (BeginTabItem(FlowGrid.Name.c_str())) {
            FlowGrid.Draw();
            EndTabItem();
        }
        if (BeginTabItem(ImGui.Name.c_str())) {
            ImGui.Draw();
            EndTabItem();
        }
        if (BeginTabItem(ImPlot.Name.c_str())) {
            ImPlot.Draw();
            EndTabItem();
        }
        EndTabBar();
    }
}

void DebugLog::Draw() const {
    ShowDebugLogWindow();
}
void StackTool::Draw() const {
    ShowStackToolWindow();
}

//-----------------------------------------------------------------------------
// [SECTION] File
//-----------------------------------------------------------------------------

static auto *file_dialog = ImGuiFileDialog::Instance();
static const string file_dialog_key = "FileDialog";

void FileDialog::Draw() const {
    if (!Visible) return file_dialog->Close();

    // `OpenDialog` is a no-op if it's already open, so it's safe to call every frame.
    file_dialog->OpenDialog(file_dialog_key, Title, string(Filters).c_str(), FilePath, DefaultFileName, MaxNumSelections, nullptr, Flags);

    const ImVec2 min_dialog_size = GetMainViewport()->Size / 2;
    if (file_dialog->Display(file_dialog_key, ImGuiWindowFlags_NoCollapse, min_dialog_size)) {
        q(close_file_dialog{}, true);
        if (file_dialog->IsOk()) {
            const fs::path &file_path = file_dialog->GetFilePathName();
            const string &extension = file_path.extension();
            if (AllProjectExtensions.find(extension) != AllProjectExtensions.end()) {
                // TODO provide an option to save with undo state.
                //   This file format would be a json list of diffs.
                //   The file would generally be larger, and the load time would be slower,
                //   but it would provide the option to save/load _exactly_ as if you'd never quit at all,
                //   with full undo/redo history/position/etc.!
                if (SaveMode) q(save_project{file_path});
                else q(open_project{file_path});
            } else if (extension == FaustDspFileExtension) {
                if (SaveMode) q(save_faust_file{file_path});
                else q(open_faust_file{file_path});
            } else {
                // todo need a way to tell it's the svg-save case
                if (SaveMode) q(save_faust_svg_file{file_path});
            }
        }
    }
}
