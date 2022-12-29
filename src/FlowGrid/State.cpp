#include "StateJson.h"

#include <fstream>

#include "imgui_memory_editor.h"
#include "implot_internal.h"

#include "FileDialog/FileDialogDemo.h"
#include "UI/Faust/FaustGraph.h"

using namespace ImGui;
using namespace fg;
using namespace action;

//-----------------------------------------------------------------------------
// [SECTION] Draw helpers
//-----------------------------------------------------------------------------

ImRect RowItemRect() {
    const ImVec2 row_min = {GetWindowPos().x, GetCursorScreenPos().y};
    return {row_min, row_min + ImVec2{GetWindowWidth(), GetFontSize()}};
}

ImRect RowItemRatioRect(float ratio) {
    const ImVec2 row_min = {GetWindowPos().x, GetCursorScreenPos().y};
    return {row_min, row_min + ImVec2{GetWindowWidth() * std::clamp(ratio, 0.f, 1.f), GetFontSize()}};
}

void FillRowItemBg(const U32 col = s.Style.ImGui.Colors[ImGuiCol_FrameBgActive]) {
    const auto &rect = RowItemRect();
    GetWindowDrawList()->AddRectFilled(rect.Min, rect.Max, col);
}

//-----------------------------------------------------------------------------
// [SECTION] Fields
//-----------------------------------------------------------------------------

namespace Field {
Base::Base(StateMember *parent, string_view id, string_view name_help, Primitive value) : UIStateMember(parent, id, name_help) {
    WithPath[Path] = this;
    Set(*this, value, c.InitStore);
}
Base::~Base() {
    WithPath.erase(Path);
}

Primitive Base::Get() const { return AppStore.at(Path); }
Primitive Base::GetInitial() const { return c.InitStore.at(Path); }

void Bool::Toggle() const { q(ToggleValue{Path}); }

void Bool::Render() const {
    bool value = Value;
    if (Checkbox(ImGuiLabel.c_str(), &value)) Toggle();
    HelpMarker();
}
bool Bool::CheckedDraw() const {
    bool value = Value;
    bool toggled = Checkbox(ImGuiLabel.c_str(), &value);
    if (toggled) Toggle();
    HelpMarker();
    return toggled;
}
void Bool::MenuItem() const {
    const bool value = Value;
    HelpMarker(false);
    if (ImGui::MenuItem(ImGuiLabel.c_str(), nullptr, value)) Toggle();
}

void UInt::Render() const {
    U32 value = Value;
    const bool edited = SliderScalar(ImGuiLabel.c_str(), ImGuiDataType_S32, &value, &Min, &Max, "%d");
    UiContext.WidgetGestured();
    if (edited) q(SetValue{Path, value});
    HelpMarker();
}
void UInt::Render(const vector<U32> &options) const {
    if (options.empty()) return;

    const U32 value = Value;
    if (BeginCombo(ImGuiLabel.c_str(), ValueName(value).c_str())) {
        for (const auto option : options) {
            const bool is_selected = option == value;
            if (Selectable(ValueName(option).c_str(), is_selected)) q(SetValue{Path, option});
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}

void UInt::ColorEdit4(ImGuiColorEditFlags flags, bool allow_auto) const {
    const Count i = std::stoi(PathSegment); // Assuming color is a member of a vector here.
    const bool is_auto = allow_auto && Value == Colors::AutoColor;
    const U32 mapped_value = is_auto ? ColorConvertFloat4ToU32(ImPlot::GetAutoColor(int(i))) : Value;

    PushID(ImGuiLabel.c_str());
    InvisibleButton({GetWindowWidth(), GetFontSize()}, ""); // todo try `Begin/EndGroup` after this works for hover info pane (over label)
    SetItemAllowOverlap();

    // todo use auto for FG colors (link to ImGui colors)
    if (allow_auto) {
        if (!is_auto) PushStyleVar(ImGuiStyleVar_Alpha, 0.25);
        if (Button("Auto")) q(SetValue{Path, is_auto ? mapped_value : Colors::AutoColor});
        if (!is_auto) PopStyleVar();
        SameLine();
    }

    auto value = ColorConvertU32ToFloat4(mapped_value);
    if (is_auto) BeginDisabled();
    const bool changed = ImGui::ColorEdit4("", (float *)&value, flags | ImGuiColorEditFlags_AlphaBar | (allow_auto ? ImGuiColorEditFlags_AlphaPreviewHalf : 0));
    UiContext.WidgetGestured();
    if (is_auto) EndDisabled();

    SameLine(0, GetStyle().ItemInnerSpacing.x);
    TextUnformatted(Name.c_str());

    PopID();

    if (changed) q(SetValue{Path, ColorConvertFloat4ToU32(value)});
}

void Int::Render() const {
    int value = Value;
    const bool edited = SliderInt(ImGuiLabel.c_str(), &value, Min, Max, "%d", ImGuiSliderFlags_None);
    UiContext.WidgetGestured();
    if (edited) q(SetValue{Path, value});
    HelpMarker();
}
void Int::Render(const vector<int> &options) const {
    if (options.empty()) return;

    const int value = Value;
    if (BeginCombo(ImGuiLabel.c_str(), to_string(value).c_str())) {
        for (const auto option : options) {
            const bool is_selected = option == value;
            if (Selectable(to_string(option).c_str(), is_selected)) q(SetValue{Path, option});
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}

void Float::Render() const {
    float value = Value;
    const bool edited = DragSpeed > 0 ? DragFloat(ImGuiLabel.c_str(), &value, DragSpeed, Min, Max, Format, Flags) : SliderFloat(ImGuiLabel.c_str(), &value, Min, Max, Format, Flags);
    UiContext.WidgetGestured();
    if (edited) q(SetValue{Path, value});
    HelpMarker();
}

void Enum::Render() const {
    Render(views::ints(0, int(Names.size())) | to<vector>); // todo if I stick with this pattern, cache.
}
void Enum::Render(const vector<int> &options) const {
    if (options.empty()) return;

    const int value = Value;
    if (BeginCombo(ImGuiLabel.c_str(), OptionName(value).c_str())) {
        for (int option : options) {
            const bool is_selected = option == value;
            const auto &name = OptionName(option);
            if (Selectable(name.c_str(), is_selected)) q(SetValue{Path, option});
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}
void Enum::MenuItem() const {
    const int value = Value;
    HelpMarker(false);
    if (BeginMenu(ImGuiLabel.c_str())) {
        for (Count i = 0; i < Names.size(); i++) {
            const bool is_selected = value == int(i);
            if (ImGui::MenuItem(Names[i].c_str(), nullptr, is_selected)) q(SetValue{Path, int(i)});
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
}

void Flags::Render() const {
    const int value = Value;
    if (TreeNodeEx(ImGuiLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        for (Count i = 0; i < Items.size(); i++) {
            const auto &item = Items[i];
            const int option_mask = 1 << i;
            bool is_selected = option_mask & value;
            if (Checkbox(item.Name.c_str(), &is_selected)) q(SetValue{Path, value ^ option_mask}); // Toggle bit
            if (!item.Help.empty()) {
                SameLine();
                ::HelpMarker(item.Help.c_str());
            }
        }
        TreePop();
    }
    HelpMarker();
}
void Flags::MenuItem() const {
    const int value = Value;
    HelpMarker(false);
    if (BeginMenu(ImGuiLabel.c_str())) {
        for (Count i = 0; i < Items.size(); i++) {
            const auto &item = Items[i];
            const int option_mask = 1 << i;
            const bool is_selected = option_mask & value;
            if (!item.Help.empty()) {
                ::HelpMarker(item.Help.c_str());
                SameLine();
            }
            if (ImGui::MenuItem(item.Name.c_str(), nullptr, is_selected)) q(SetValue{Path, value ^ option_mask}); // Toggle bit
            if (is_selected) SetItemDefaultFocus();
        }
        EndMenu();
    }
}

void String::Render() const {
    const string value = Value;
    TextUnformatted(value.c_str());
}
void String::Render(const vector<string> &options) const {
    if (options.empty()) return;

    const string value = *this;
    if (BeginCombo(ImGuiLabel.c_str(), value.c_str())) {
        for (const auto &option : options) {
            const bool is_selected = option == value;
            if (Selectable(option.c_str(), is_selected)) q(SetValue{Path, option});
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}

} // namespace Field

template<IsPrimitive T>
T Vector<T>::operator[](Count i) const { return std::get<T>(AppStore.at(Path / to_string(i))); };

template<IsPrimitive T>
Count Vector<T>::Size(const Store &store) const {
    Count i = 0;
    while (store.count(Path / to_string(i++))) {}
    return i - 1;
}

template<IsPrimitive T>
void Vector<T>::Set(const vector<T> &values, TransientStore &store) const {
    Count i = 0;
    while (i < values.size()) {
        store.set(Path / to_string(i), T(values[i])); // When T is a bool, an explicit cast seems to be needed?
        i++;
    }

    while (store.count(Path / to_string(i))) store.erase(Path / to_string(i++));
}

template<IsPrimitive T>
void Vector<T>::Set(const vector<pair<int, T>> &values, TransientStore &store) const {
    for (const auto &[i, value] : values) store.set(Path / to_string(i), value);
}

template<IsPrimitive T>
T Vector2D<T>::At(Count i, Count j, const Store &store) const { return std::get<T>(store.at(Path / to_string(i) / to_string(j))); };

template<IsPrimitive T>
Count Vector2D<T>::Size(const TransientStore &store) const {
    Count i = 0;
    while (store.count(Path / i++ / 0).to_string()) {}
    return i - 1;
}

template<IsPrimitive T>
void Vector2D<T>::Set(const vector<vector<T>> &values, TransientStore &store) const {
    Count i = 0;
    while (i < values.size()) {
        Count j = 0;
        while (j < values[i].size()) {
            store.set(Path / to_string(i) / to_string(j), T(values[i][j]));
            j++;
        }
        while (store.count(Path / to_string(i) / to_string(j))) store.erase(Path / to_string(i) / to_string(j++));
        i++;
    }

    while (store.count(Path / to_string(i) / "0")) {
        Count j = 0;
        while (store.count(Path / to_string(i) / to_string(j))) store.erase(Path / to_string(i) / to_string(j++));
        i++;
    }
}

void Vec2::Render(ImGuiSliderFlags flags) const {
    ImVec2 values = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&values, X.Min, X.Max, Format, flags);
    UiContext.WidgetGestured();
    if (edited) q(SetValues{{{X.Path, values.x}, {Y.Path, values.y}}});
    HelpMarker();
}

void Vec2::Render() const { Render(ImGuiSliderFlags_None); }

void Vec2Linked::Render(ImGuiSliderFlags flags) const {
    PushID(ImGuiLabel.c_str());
    if (Linked.CheckedDraw()) {
        // Linking sets the max value to the min value.
        if (X < Y) q(SetValue{Y.Path, X});
        else if (Y < X) q(SetValue{X.Path, Y});
    }
    PopID();
    SameLine();
    ImVec2 values = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&values, X.Min, X.Max, Format, flags);
    UiContext.WidgetGestured();
    if (edited) {
        if (Linked) {
            const float changed_value = values.x != X ? values.x : values.y;
            q(SetValues{{{X.Path, changed_value}, {Y.Path, changed_value}}});
        } else {
            q(SetValues{{{X.Path, values.x}, {Y.Path, values.y}}});
        }
    }
    HelpMarker();
}

void Vec2Linked::Render() const { Render(ImGuiSliderFlags_None); }

// Currently, `Draw` is not used for anything except wrapping around `Render`.
// Fields don't wrap their `Render` with a push/pop-id, ImGui widgets all push the provided label to the ID stack.
void Drawable::Draw() const {
    //    PushID(ImGuiLabel.c_str());
    Render();
    //    PopID();
}

// Helper to display a (?) mark which shows a tooltip when hovered. From `imgui_demo.cpp`.
void StateMember::HelpMarker(const bool after) const {
    if (Help.empty()) return;

    if (after) SameLine();
    ::HelpMarker(Help.c_str());
    if (!after) SameLine();
}

//-----------------------------------------------------------------------------
// [SECTION] Window/tabs methods
//-----------------------------------------------------------------------------

Window::Window(StateMember *parent, string_view path_segment, string_view name_help, const bool visible)
    : UIStateMember(parent, path_segment, name_help) {
    Set(Visible, visible, c.InitStore);
}
Window::Window(StateMember *parent, string_view path_segment, string_view name_help, const ImGuiWindowFlags flags)
    : UIStateMember(parent, path_segment, name_help), WindowFlags(flags) {}
Window::Window(StateMember *parent, string_view path_segment, string_view name_help, Menu menu)
    : UIStateMember(parent, path_segment, name_help), WindowMenu{std::move(menu)} {}

void Window::Draw() const {
    if (!Visible) return;

    ImGuiWindowFlags flags = WindowFlags;
    if (!WindowMenu.Items.empty()) flags |= ImGuiWindowFlags_MenuBar;

    bool open = Visible;
    if (Begin(ImGuiLabel.c_str(), &open, flags) && open) {
        WindowMenu.Draw();
        Render();
    }
    End();

    if (Visible && !open) q(SetValue{Visible.Path, false});
}

void Window::Dock(ID node_id) const {
    DockBuilderDockWindow(ImGuiLabel.c_str(), node_id);
}

void Window::MenuItem() const {
    if (ImGui::MenuItem(ImGuiLabel.c_str(), nullptr, Visible)) q(ToggleValue{Visible.Path});
}

void Window::SelectTab() const {
    FindImGuiWindow().DockNode->SelectedTabId = FindImGuiWindow().TabId;
}

void TabsWindow::Render() const {
    if (BeginTabBar("")) {
        for (const auto *child : Children) {
            if (const auto *ui_child = dynamic_cast<const UIStateMember *>(child)) {
                if (ui_child != &Visible && BeginTabItem(child->ImGuiLabel.c_str())) {
                    ui_child->Draw();
                    EndTabItem();
                }
            }
        }
        EndTabBar();
    }
}

void Menu::Render() const {
    if (Items.empty()) return;

    const bool is_menu_bar = Label.empty();
    if (IsMain ? BeginMainMenuBar() : (is_menu_bar ? BeginMenuBar() : BeginMenu(Label.c_str()))) {
        for (const auto &item : Items) {
            Match(
                item,
                [](const Menu &menu) {
                    menu.Draw();
                },
                [](const MenuItemDrawable &drawable) {
                    drawable.MenuItem();
                },
                [](const EmptyAction &action) {
                    const string menu_label = action::GetMenuLabel(action);
                    const string shortcut = action::GetShortcut(action);
                    if (ImGui::MenuItem(menu_label.c_str(), shortcut.c_str(), false, c.ActionAllowed(action))) {
                        Match(action, [](const auto &a) { q(a); });
                    }
                },
            );
        }
        if (IsMain) EndMainMenuBar();
        else if (is_menu_bar) EndMenuBar();
        else EndMenu();
    }
}

void Info::Render() const {
    const auto hovered_id = GetHoveredID();
    if (!hovered_id) return;

    PushTextWrapPos(0);
    if (StateMember::WithId.contains(hovered_id)) {
        const auto *member = StateMember::WithId.at(hovered_id);
        const string help = member->Help.empty() ? format("No info available for \"{}\".", member->Name) : member->Help;
        TextUnformatted(help.c_str());
    } else if (Box box = GetHoveredBox(hovered_id)) {
        TextUnformatted(GetTreeInfo(box).c_str());
    }
    PopTextWrapPos();
}

void State::UIProcess::Render() const {}

void OpenRecentProject::MenuItem() const {
    if (BeginMenu("Open recent project", !c.Preferences.RecentlyOpenedPaths.empty())) {
        for (const auto &recently_opened_path : c.Preferences.RecentlyOpenedPaths) {
            if (ImGui::MenuItem(recently_opened_path.filename().c_str())) q(OpenProject{recently_opened_path});
        }
        EndMenu();
    }
}

void State::Render() const {
    MainMenu.Draw();

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
        Audio.Faust.Graph.Dock(faust_tools_node_id);
        Audio.Faust.Params.Dock(faust_tools_node_id);

        DebugLog.Dock(debug_node_id);
        StackTool.Dock(debug_node_id);
        Audio.Faust.Log.Dock(debug_node_id);
        StateViewer.Dock(debug_node_id);
        StateMemoryEditor.Dock(debug_node_id);
        StatePathUpdateFrequency.Dock(debug_node_id);
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

    for (const auto *child : Children) {
        if (const auto *ui_child = dynamic_cast<const UIStateMember *>(child)) {
            ui_child->Draw();
        }
    }
}

// Copy of ImGui version, which is not defined publicly
struct ImGuiDockNodeSettings { // NOLINT(cppcoreguidelines-pro-type-member-init)
    ID NodeId;
    ID ParentNodeId;
    ID ParentWindowId;
    ID SelectedTabId;
    S8 SplitAxis;
    char Depth;
    ImGuiDockNodeFlags Flags;
    ImVec2ih Pos;
    ImVec2ih Size;
    ImVec2ih SizeRef;
};

void DockNodeSettings::Set(const ImVector<ImGuiDockNodeSettings> &dss, TransientStore &store) const {
    const Count size = dss.Size;
    vector<ID> node_id(size), parent_node_id(size), parent_window_id(size), selected_tab_id(size);
    vector<int> split_axis(size), depth(size), flags(size);
    vector<U32> pos(size), sz(size), sz_ref(size);
    for (Count i = 0; i < size; i++) {
        const auto &ds = dss[int(i)];
        node_id[i] = ds.NodeId;
        parent_node_id[i] = ds.ParentNodeId;
        parent_window_id[i] = ds.ParentWindowId;
        selected_tab_id[i] = ds.SelectedTabId;
        split_axis[i] = ds.SplitAxis;
        depth[i] = ds.Depth;
        flags[i] = ds.Flags;
        pos[i] = PackImVec2ih(ds.Pos);
        sz[i] = PackImVec2ih(ds.Size);
        sz_ref[i] = PackImVec2ih(ds.SizeRef);
    }
    NodeId.Set(node_id, store);
    ParentNodeId.Set(parent_node_id, store);
    ParentWindowId.Set(parent_window_id, store);
    SelectedTabId.Set(selected_tab_id, store);
    SplitAxis.Set(split_axis, store);
    Depth.Set(depth, store);
    Flags.Set(flags, store);
    Pos.Set(pos, store);
    Size.Set(sz, store);
    SizeRef.Set(sz_ref, store);
}
void DockNodeSettings::Apply(ImGuiContext *ctx) const {
    // Assumes `DockSettingsHandler_ClearAll` has already been called.
    const auto size = NodeId.Size();
    for (Count i = 0; i < size; i++) {
        ctx->DockContext.NodesSettings.push_back({
            NodeId[i],
            ParentNodeId[i],
            ParentWindowId[i],
            SelectedTabId[i],
            S8(SplitAxis[i]),
            char(Depth[i]),
            Flags[i],
            UnpackImVec2ih(Pos[i]),
            UnpackImVec2ih(Size[i]),
            UnpackImVec2ih(SizeRef[i]),
        });
    }
}

void WindowSettings::Set(ImChunkStream<ImGuiWindowSettings> &wss, TransientStore &store) const {
    vector<ID> id, class_id, viewport_id, dock_id;
    vector<int> dock_order;
    vector<U32> pos, sz, viewport_pos;
    vector<bool> collapsed;
    for (auto *ws = wss.begin(); ws != nullptr; ws = wss.next_chunk(ws)) {
        id.push_back(ws->ID);
        class_id.push_back(ws->ClassId);
        viewport_id.push_back(ws->ViewportId);
        dock_id.push_back(ws->DockId);
        dock_order.push_back(ws->DockOrder);
        pos.push_back(PackImVec2ih(ws->Pos));
        sz.push_back(PackImVec2ih(ws->Size));
        viewport_pos.push_back(PackImVec2ih(ws->ViewportPos));
        collapsed.push_back(ws->Collapsed);
    }
    Id.Set(id, store);
    ClassId.Set(class_id, store);
    ViewportId.Set(viewport_id, store);
    DockId.Set(dock_id, store);
    DockOrder.Set(dock_order, store);
    Pos.Set(pos, store);
    Size.Set(sz, store);
    ViewportPos.Set(viewport_pos, store);
    Collapsed.Set(collapsed, store);
}

// See `imgui.cpp::ApplyWindowSettings`
void WindowSettings::Apply(ImGuiContext *) const {
    const auto *main_viewport = GetMainViewport();
    const auto size = Id.Size();
    for (Count i = 0; i < size; i++) {
        const auto id = Id[i];
        auto *window = FindWindowByID(id);
        if (!window) {
            cout << "Unable to apply settings for window with ID " << format("{:#08X}", id) << ": Window not found.\n";
            continue;
        }

        window->ViewportPos = main_viewport->Pos;
        if (ViewportId[i]) {
            window->ViewportId = ViewportId[i];
            const auto viewport_pos = UnpackImVec2ih(ViewportPos[i]);
            window->ViewportPos = ImVec2(viewport_pos.x, viewport_pos.y);
        }
        const auto pos = UnpackImVec2ih(Pos[i]);
        window->Pos = ImVec2(pos.x, pos.y) + ImFloor(window->ViewportPos);

        const auto size = UnpackImVec2ih(Size[i]);
        if (size.x > 0 && size.y > 0) window->Size = window->SizeFull = ImVec2(size.x, size.y);
        window->Collapsed = Collapsed[i];
        window->DockId = DockId[i];
        window->DockOrder = short(DockOrder[i]);
    }
}

void TableSettings::Set(ImChunkStream<ImGuiTableSettings> &tss, TransientStore &store) const {
    // Table settings
    vector<ImGuiID> id;
    vector<int> save_flags;
    vector<float> ref_scale;
    vector<Count> columns_counts, columns_count_max;
    vector<bool> want_apply;

    // Column settings
    vector<vector<float>> width_or_weight;
    vector<vector<::ID>> user_id;
    vector<vector<int>> index, display_order, sort_order, sort_direction;
    vector<vector<bool>> is_enabled, is_stretch;

    for (auto *ts_it = tss.begin(); ts_it != nullptr; ts_it = tss.next_chunk(ts_it)) {
        auto &ts = *ts_it;
        const auto columns_count = ts.ColumnsCount;

        id.push_back(ts.ID);
        save_flags.push_back(ts.SaveFlags);
        ref_scale.push_back(ts.RefScale);
        columns_counts.push_back(columns_count);
        columns_count_max.push_back(ts.ColumnsCountMax);
        want_apply.push_back(ts.WantApply);

        width_or_weight.push_back(vector<float>(columns_count));
        user_id.push_back(vector<::ID>(columns_count));
        index.push_back(vector<int>(columns_count));
        display_order.push_back(vector<int>(columns_count));
        sort_order.push_back(vector<int>(columns_count));
        sort_direction.push_back(vector<int>(columns_count));
        is_enabled.push_back(vector<bool>(columns_count));
        is_stretch.push_back(vector<bool>(columns_count));

        for (int column_index = 0; column_index < columns_count; column_index++) {
            const auto &cs = ts.GetColumnSettings()[column_index];
            width_or_weight.back()[column_index] = cs.WidthOrWeight;
            user_id.back()[column_index] = cs.UserID;
            index.back()[column_index] = cs.Index;
            display_order.back()[column_index] = cs.DisplayOrder;
            sort_order.back()[column_index] = cs.SortOrder;
            sort_direction.back()[column_index] = cs.SortDirection;
            is_enabled.back()[column_index] = cs.IsEnabled;
            is_stretch.back()[column_index] = cs.IsStretch;
        }
    }

    ID.Set(id, store);
    SaveFlags.Set(save_flags, store);
    RefScale.Set(ref_scale, store);
    ColumnsCount.Set(columns_counts, store);
    ColumnsCountMax.Set(columns_count_max, store);
    WantApply.Set(want_apply, store);

    Columns.WidthOrWeight.Set(width_or_weight, store);
    Columns.UserID.Set(user_id, store);
    Columns.Index.Set(index, store);
    Columns.DisplayOrder.Set(display_order, store);
    Columns.SortOrder.Set(sort_order, store);
    Columns.SortDirection.Set(sort_direction, store);
    Columns.IsEnabled.Set(is_enabled, store);
    Columns.IsStretch.Set(is_stretch, store);
}

// Adapted from `imgui_tables.cpp::TableLoadSettings`
void TableSettings::Apply(ImGuiContext *) const {
    const auto size = ID.Size();
    for (Count i = 0; i < size; i++) {
        const auto id = ID[i];
        const auto table = TableFindByID(id);
        if (!table) {
            cout << "Unable to apply settings for table with ID " << format("{:#08X}", id) << ": Table not found.\n";
            continue;
        }

        table->IsSettingsRequestLoad = false; // todo remove this var/behavior?
        table->SettingsLoadedFlags = SaveFlags[i]; // todo remove this var/behavior?
        table->RefScale = RefScale[i];

        // Serialize ImGuiTableSettings/ImGuiTableColumnSettings into ImGuiTable/ImGuiTableColumn
        ImU64 display_order_mask = 0;
        for (Count j = 0; j < ColumnsCount[i]; j++) {
            int column_n = Columns.Index.At(i, j);
            if (column_n < 0 || column_n >= table->ColumnsCount) continue;

            ImGuiTableColumn *column = &table->Columns[column_n];
            if (ImGuiTableFlags(SaveFlags[i]) & ImGuiTableFlags_Resizable) {
                float width_or_weight = Columns.WidthOrWeight.At(i, j);
                if (Columns.IsStretch.At(i, j)) column->StretchWeight = width_or_weight;
                else column->WidthRequest = width_or_weight;
                column->AutoFitQueue = 0x00;
            }
            column->DisplayOrder = ImGuiTableFlags(SaveFlags[i]) & ImGuiTableFlags_Reorderable ? ImGuiTableColumnIdx(Columns.DisplayOrder.At(i, j)) : (ImGuiTableColumnIdx)column_n;
            display_order_mask |= (ImU64)1 << column->DisplayOrder;
            column->IsUserEnabled = column->IsUserEnabledNextFrame = Columns.IsEnabled.At(i, j);
            column->SortOrder = ImGuiTableColumnIdx(Columns.SortOrder.At(i, j));
            column->SortDirection = Columns.SortDirection.At(i, j);
        }

        // Validate and fix invalid display order data
        const ImU64 expected_display_order_mask = ColumnsCount[i] == 64 ? ~0 : ((ImU64)1 << ImU8(ColumnsCount[i])) - 1;
        if (display_order_mask != expected_display_order_mask) {
            for (int column_n = 0; column_n < table->ColumnsCount; column_n++) {
                table->Columns[column_n].DisplayOrder = (ImGuiTableColumnIdx)column_n;
            }
        }
        // Rebuild index
        for (int column_n = 0; column_n < table->ColumnsCount; column_n++) {
            table->DisplayOrderToIndex[table->Columns[column_n].DisplayOrder] = (ImGuiTableColumnIdx)column_n;
        }
    }
}

Store ImGuiSettings::Set(ImGuiContext *ctx) const {
    SaveIniSettingsToMemory(); // Populates the `Settings` context members

    auto store = AppStore.transient();
    Nodes.Set(ctx->DockContext.NodesSettings, store);
    Windows.Set(ctx->SettingsWindows, store);
    Tables.Set(ctx->SettingsTables, store);
    return store.persistent();
}

void ImGuiSettings::Apply(ImGuiContext *ctx) const {
    DockSettingsHandler_ClearAll(ctx, nullptr);
    Windows.Apply(ctx);
    Tables.Apply(ctx);
    Nodes.Apply(ctx);
    DockSettingsHandler_ApplyAll(ctx, nullptr);

    // Other housekeeping to emulate `LoadIniSettingsFromMemory`
    ctx->SettingsLoaded = true;
    ctx->SettingsDirty = false;
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
    for (int i = 0; i < ImGuiCol_COUNT; i++) style.Colors[i] = ColorConvertU32ToFloat4(Colors[i]);
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
    style.Colormap = ImPlotColormap_Deep; // todo configurable
    style.UseLocalTime = UseLocalTime;
    style.UseISO8601 = UseISO8601;
    style.Use24HourClock = Use24HourClock;
    for (int i = 0; i < ImPlotCol_COUNT; i++) style.Colors[i] = Colors::ConvertU32ToFloat4(Colors[i]);
    ImPlot::BustItemCache();
}

void State::Apply(const UIContext::Flags flags) const {
    if (flags == UIContext::Flags_None) return;

    if (flags & UIContext::Flags_ImGuiSettings) ImGuiSettings.Apply(UiContext.ImGui);
    if (flags & UIContext::Flags_ImGuiStyle) Style.ImGui.Apply(UiContext.ImGui);
    if (flags & UIContext::Flags_ImPlotStyle) Style.ImPlot.Apply(UiContext.ImPlot);
}

//-----------------------------------------------------------------------------
// [SECTION] State windows
//-----------------------------------------------------------------------------

// TODO option to indicate relative update-recency
void StateViewer::StateJsonTree(string_view key, const json &value, const StatePath &path) const {
    const string leaf_name = path == RootPath ? path.string() : path.filename().string();
    const auto &parent_path = path == RootPath ? path : path.parent_path();
    const bool is_array_item = IsInteger(leaf_name);
    const int array_index = is_array_item ? std::stoi(leaf_name) : -1;
    const bool is_imgui_color = parent_path == s.Style.ImGui.Colors.Path;
    const bool is_implot_color = parent_path == s.Style.ImPlot.Colors.Path;
    const bool is_flowgrid_color = parent_path == s.Style.FlowGrid.Colors.Path;
    const string label = LabelMode == Annotated ?
        (is_imgui_color        ? s.Style.ImGui.Colors.Child(array_index)->Name :
             is_implot_color   ? s.Style.ImPlot.Colors.Child(array_index)->Name :
             is_flowgrid_color ? s.Style.FlowGrid.Colors.Child(array_index)->Name :
             is_array_item     ? leaf_name :
                                 string(key)) :
        string(key);

    if (AutoSelect) {
        const auto &update_paths = c.History.LatestUpdatedPaths;
        const auto is_ancestor_path = [&path](const string &candidate_path) { return candidate_path.rfind(path.string(), 0) == 0; };
        const bool was_recently_updated = std::find_if(update_paths.begin(), update_paths.end(), is_ancestor_path) != update_paths.end();
        SetNextItemOpen(was_recently_updated);
    }

    // Flash background color of nodes when its corresponding path updates.
    const auto &latest_update_time = c.History.LatestUpdateTime(path);
    if (latest_update_time) {
        const float flash_elapsed_ratio = fsec(Clock::now() - *latest_update_time).count() / s.Style.FlowGrid.FlashDurationSec;
        ImColor flash_color = s.Style.FlowGrid.Colors[FlowGridCol_GestureIndicator];
        flash_color.Value.w = max(0.f, 1 - flash_elapsed_ratio);
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
                StateJsonTree(it.key(), *it, path / it.key());
            }
            TreePop();
        }
    } else if (value.is_array()) {
        if (JsonTreeNode(label, flags)) {
            Count i = 0;
            for (const auto &it : value) {
                StateJsonTree(to_string(i), it, path / to_string(i));
                i++;
            }
            TreePop();
        }
    } else {
        Text("%s: %s", label.c_str(), value.dump().c_str());
    }
}

void StateViewer::Render() const {
    StateJsonTree("State", c.GetProjectJson());
}

void StateMemoryEditor::Render() const {
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

void StatePathUpdateFrequency::Render() const {
    auto [labels, values] = c.History.StatePathUpdateFrequencyPlottable();
    if (labels.empty()) {
        Text("No state updates yet.");
        return;
    }

    if (ImPlot::BeginPlot("Path update frequency", {-1, float(labels.size()) * 30 + 60}, ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Number of updates", nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert);

        // Hack to allow `SetupAxisTicks` without breaking on assert `n_ticks > 1`: Just add an empty label and only plot one value.
        // todo fix in ImPlot
        if (labels.size() == 1) labels.emplace_back("");

        // todo add an axis flag to exclude non-integer ticks
        // todo add an axis flag to show last tick
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, double(labels.size() - 1), int(labels.size()), labels.data(), false);
        static const char *ItemLabels[] = {"Committed updates", "Active updates"};
        const int item_count = !c.History.ActiveGesture.empty() ? 2 : 1;
        const int group_count = int(values.size()) / item_count;
        ImPlot::PlotBarGroups(ItemLabels, values.data(), item_count, group_count, 0.75, 0, ImPlotBarGroupsFlags_Horizontal | ImPlotBarGroupsFlags_Stacked);

        ImPlot::EndPlot();
    }
}

void ProjectPreview::Render() const {
    Format.Draw();
    Raw.Draw();

    Separator();

    const json project_json = c.GetProjectJson(ProjectFormat(int(Format)));
    if (Raw) TextUnformatted(project_json.dump(4).c_str());
    else JsonTree("", project_json, JsonTreeNodeFlags_DefaultOpen);
}

//-----------------------------------------------------------------------------
// [SECTION] Style editors
//-----------------------------------------------------------------------------

Style::ImGuiStyle::ImGuiStyle(StateMember *parent, string_view path_segment, string_view name_help)
    : UIStateMember(parent, path_segment, name_help) {
    ColorsDark(c.InitStore);
}
Style::ImPlotStyle::ImPlotStyle(StateMember *parent, string_view path_segment, string_view name_help)
    : UIStateMember(parent, path_segment, name_help) {
    ColorsAuto(c.InitStore);
}
Style::FlowGridStyle::FlowGridStyle(StateMember *parent, string_view path_segment, string_view name_help)
    : UIStateMember(parent, path_segment, name_help) {
    ColorsDark(c.InitStore);
}
Style::FlowGridStyle::Graph::Graph(StateMember *parent, string_view path_segment, string_view name_help)
    : UIStateMember(parent, path_segment, name_help) {
    ColorsDark(c.InitStore);
    LayoutFlowGrid(c.InitStore);
}

void Style::ImGuiStyle::ColorsDark(TransientStore &store) const {
    ImGui::StyleColorsDark(&ColorPresetBuffer[0]);
    Colors.Set(ColorPresetBuffer, store);
}
void Style::ImGuiStyle::ColorsLight(TransientStore &store) const {
    ImGui::StyleColorsLight(&ColorPresetBuffer[0]);
    Colors.Set(ColorPresetBuffer, store);
}
void Style::ImGuiStyle::ColorsClassic(TransientStore &store) const {
    ImGui::StyleColorsClassic(&ColorPresetBuffer[0]);
    Colors.Set(ColorPresetBuffer, store);
}

void Style::ImPlotStyle::ColorsAuto(TransientStore &store) const {
    ImPlot::StyleColorsAuto(&ColorPresetBuffer[0]);
    Colors.Set(ColorPresetBuffer, store);
    Set(MinorAlpha, 0.25f, store);
}
void Style::ImPlotStyle::ColorsDark(TransientStore &store) const {
    ImPlot::StyleColorsDark(&ColorPresetBuffer[0]);
    Colors.Set(ColorPresetBuffer, store);
    Set(MinorAlpha, 0.25f, store);
}
void Style::ImPlotStyle::ColorsLight(TransientStore &store) const {
    ImPlot::StyleColorsLight(&ColorPresetBuffer[0]);
    Colors.Set(ColorPresetBuffer, store);
    Set(MinorAlpha, 1, store);
}
void Style::ImPlotStyle::ColorsClassic(TransientStore &store) const {
    ImPlot::StyleColorsClassic(&ColorPresetBuffer[0]);
    Colors.Set(ColorPresetBuffer, store);
    Set(MinorAlpha, 0.5f, store);
}

void Style::FlowGridStyle::ColorsDark(TransientStore &store) const {
    Colors.Set(
        {
            {FlowGridCol_HighlightText, {1, 0.6, 0, 1}},
            {FlowGridCol_GestureIndicator, {0.87, 0.52, 0.32, 1}},
            {FlowGridCol_ParamsBg, {0.16, 0.29, 0.48, 0.1}},
        },
        store
    );
}
void Style::FlowGridStyle::ColorsLight(TransientStore &store) const {
    Colors.Set(
        {
            {FlowGridCol_HighlightText, {1, 0.45, 0, 1}},
            {FlowGridCol_GestureIndicator, {0.87, 0.52, 0.32, 1}},
            {FlowGridCol_ParamsBg, {1, 1, 1, 1}},
        },
        store
    );
}
void Style::FlowGridStyle::ColorsClassic(TransientStore &store) const {
    Colors.Set(
        {
            {FlowGridCol_HighlightText, {1, 0.6, 0, 1}},
            {FlowGridCol_GestureIndicator, {0.87, 0.52, 0.32, 1}},
            {FlowGridCol_ParamsBg, {0.43, 0.43, 0.43, 0.1}},
        },
        store
    );
}

void Style::FlowGridStyle::Graph::ColorsDark(TransientStore &store) const {
    Colors.Set(
        {
            {FlowGridGraphCol_Bg, {0.06, 0.06, 0.06, 0.94}},
            {FlowGridGraphCol_Text, {1, 1, 1, 1}},
            {FlowGridGraphCol_DecorateStroke, {0.43, 0.43, 0.5, 0.5}},
            {FlowGridGraphCol_GroupStroke, {0.43, 0.43, 0.5, 0.5}},
            {FlowGridGraphCol_Line, {0.61, 0.61, 0.61, 1}},
            {FlowGridGraphCol_Link, {0.26, 0.59, 0.98, 0.4}},
            {FlowGridGraphCol_Inverter, {1, 1, 1, 1}},
            {FlowGridGraphCol_OrientationMark, {1, 1, 1, 1}},
            // Box fills
            {FlowGridGraphCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridGraphCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridGraphCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridGraphCol_Number, {0.96, 0.28, 0, 1}},
        },
        store
    );
}
void Style::FlowGridStyle::Graph::ColorsClassic(TransientStore &store) const {
    Colors.Set(
        {
            {FlowGridGraphCol_Bg, {0, 0, 0, 0.85}},
            {FlowGridGraphCol_Text, {0.9, 0.9, 0.9, 1}},
            {FlowGridGraphCol_DecorateStroke, {0.5, 0.5, 0.5, 0.5}},
            {FlowGridGraphCol_GroupStroke, {0.5, 0.5, 0.5, 0.5}},
            {FlowGridGraphCol_Line, {1, 1, 1, 1}},
            {FlowGridGraphCol_Link, {0.35, 0.4, 0.61, 0.62}},
            {FlowGridGraphCol_Inverter, {0.9, 0.9, 0.9, 1}},
            {FlowGridGraphCol_OrientationMark, {0.9, 0.9, 0.9, 1}},
            // Box fills
            {FlowGridGraphCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridGraphCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridGraphCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridGraphCol_Number, {0.96, 0.28, 0, 1}},
        },
        store
    );
}
void Style::FlowGridStyle::Graph::ColorsLight(TransientStore &store) const {
    Colors.Set(
        {
            {FlowGridGraphCol_Bg, {0.94, 0.94, 0.94, 1}},
            {FlowGridGraphCol_Text, {0, 0, 0, 1}},
            {FlowGridGraphCol_DecorateStroke, {0, 0, 0, 0.3}},
            {FlowGridGraphCol_GroupStroke, {0, 0, 0, 0.3}},
            {FlowGridGraphCol_Line, {0.39, 0.39, 0.39, 1}},
            {FlowGridGraphCol_Link, {0.26, 0.59, 0.98, 0.4}},
            {FlowGridGraphCol_Inverter, {0, 0, 0, 1}},
            {FlowGridGraphCol_OrientationMark, {0, 0, 0, 1}},
            // Box fills
            {FlowGridGraphCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridGraphCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridGraphCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridGraphCol_Number, {0.96, 0.28, 0, 1}},
        },
        store
    );
}
void Style::FlowGridStyle::Graph::ColorsFaust(TransientStore &store) const {
    Colors.Set(
        {
            {FlowGridGraphCol_Bg, {1, 1, 1, 1}},
            {FlowGridGraphCol_Text, {1, 1, 1, 1}},
            {FlowGridGraphCol_DecorateStroke, {0.2, 0.2, 0.2, 1}},
            {FlowGridGraphCol_GroupStroke, {0.2, 0.2, 0.2, 1}},
            {FlowGridGraphCol_Line, {0, 0, 0, 1}},
            {FlowGridGraphCol_Link, {0, 0.2, 0.4, 1}},
            {FlowGridGraphCol_Inverter, {0, 0, 0, 1}},
            {FlowGridGraphCol_OrientationMark, {0, 0, 0, 1}},
            // Box fills
            {FlowGridGraphCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridGraphCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridGraphCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridGraphCol_Number, {0.96, 0.28, 0, 1}},
        },
        store
    );
}

void Style::FlowGridStyle::Graph::LayoutFlowGrid(TransientStore &store) const {
    Set(DefaultLayoutEntries, store);
}
void Style::FlowGridStyle::Graph::LayoutFaust(TransientStore &store) const {
    Set(
        {
            {SequentialConnectionZigzag, true},
            {OrientationMark, true},
            {DecorateRootNode, true},
            {DecorateMargin.X, 10},
            {DecorateMargin.Y, 10},
            {DecoratePadding.X, 10},
            {DecoratePadding.Y, 10},
            {DecorateLineWidth, 1},
            {DecorateCornerRadius, 0},
            {GroupMargin.X, 10},
            {GroupMargin.Y, 10},
            {GroupPadding.X, 10},
            {GroupPadding.Y, 10},
            {GroupLineWidth, 1},
            {GroupCornerRadius, 0},
            {BoxCornerRadius, 0},
            {BinaryHorizontalGapRatio, 0.25f},
            {WireWidth, 1},
            {WireGap, 16},
            {NodeMargin.X, 8},
            {NodeMargin.Y, 8},
            {NodePadding.X, 8},
            {NodePadding.Y, 0},
            {ArrowSize.X, 3},
            {ArrowSize.Y, 2},
            {InverterRadius, 3},
        },
        store
    );
}

const UInt *Colors::At(Count i) const { return dynamic_cast<const UInt *>(Children[i]); }
U32 Colors::operator[](Count i) const { return *At(i); };
void Colors::Set(const vector<ImVec4> &values, TransientStore &transient) const {
    for (Count i = 0; i < values.size(); i++) {
        ::Set(*At(i), ConvertFloat4ToU32(values[i]), transient);
    }
}
void Colors::Set(const vector<pair<int, ImVec4>> &entries, TransientStore &transient) const {
    for (const auto &[i, v] : entries) {
        ::Set(*At(i), ConvertFloat4ToU32(v), transient);
    }
}

void Colors::Render() const {
    static ImGuiTextFilter filter;
    filter.Draw("Filter colors", GetFontSize() * 16);

    static ImGuiColorEditFlags flags = 0;
    if (RadioButton("Opaque", flags == ImGuiColorEditFlags_None)) flags = ImGuiColorEditFlags_None;
    SameLine();
    if (RadioButton("Alpha", flags == ImGuiColorEditFlags_AlphaPreview)) flags = ImGuiColorEditFlags_AlphaPreview;
    SameLine();
    if (RadioButton("Both", flags == ImGuiColorEditFlags_AlphaPreviewHalf)) flags = ImGuiColorEditFlags_AlphaPreviewHalf;
    SameLine();
    ::HelpMarker("In the color list:\n"
                 "Left-click on color square to open color picker.\n"
                 "Right-click to open edit options menu.");

    BeginChild("##colors", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_NavFlattened);
    PushItemWidth(-160);

    for (const auto *child : Children) {
        const auto *child_color = dynamic_cast<const UInt *>(child);
        if (filter.PassFilter(child->Name.c_str())) {
            child_color->ColorEdit4(flags, AllowAuto);
        }
    }
    if (AllowAuto) {
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

void Style::ImGuiStyle::Render() const {
    static int style_idx = -1;
    if (Combo("Colors##Selector", &style_idx, "Dark\0Light\0Classic\0")) q(SetImGuiColorStyle{style_idx});

    const auto &io = GetIO();
    const auto *font_current = GetFont();
    if (BeginCombo("Fonts", font_current->GetDebugName())) {
        for (int n = 0; n < io.Fonts->Fonts.Size; n++) {
            const auto *font = io.Fonts->Fonts[n];
            PushID(font);
            if (Selectable(font->GetDebugName(), font == font_current)) q(SetValue{FontIndex.Path, n});
            PopID();
        }
        EndCombo();
    }

    // Simplified Settings (expose floating-pointer border sizes as boolean representing 0 or 1)
    {
        bool border = WindowBorderSize > 0;
        if (Checkbox("WindowBorder", &border)) q(SetValue{WindowBorderSize.Path, border ? 1 : 0});
    }
    SameLine();
    {
        bool border = FrameBorderSize > 0;
        if (Checkbox("FrameBorder", &border)) q(SetValue{FrameBorderSize.Path, border ? 1 : 0});
    }
    SameLine();
    {
        bool border = PopupBorderSize > 0;
        if (Checkbox("PopupBorder", &border)) q(SetValue{PopupBorderSize.Path, border ? 1 : 0});
    }

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

                    const float canvas_width = max(min_widget_width, rad * 2);
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
    if (Combo("Colors##Selector", &style_idx, "Auto\0Dark\0Light\0Classic\0")) q(SetImPlotColorStyle{style_idx});

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
        }
        EndTabBar();
    }
}

void Style::FlowGridStyle::Graph::Render() const {
    FoldComplexity.Draw();
    const bool scale_fill = ScaleFillHeight;
    ScaleFillHeight.Draw();
    if (scale_fill) BeginDisabled();
    Scale.Draw();
    if (scale_fill) {
        SameLine();
        TextUnformatted(format("Uncheck '{}' to manually edit graph scale.", ScaleFillHeight.Name).c_str());
        EndDisabled();
    }
    Direction.Draw();
    OrientationMark.Draw();
    if (OrientationMark) {
        SameLine();
        SetNextItemWidth(GetContentRegionAvail().x * 0.5f);
        OrientationMarkRadius.Draw();
    }
    RouteFrame.Draw();
    SequentialConnectionZigzag.Draw();
    Separator();
    const bool decorate_folded = DecorateRootNode;
    DecorateRootNode.Draw();
    if (!decorate_folded) BeginDisabled();
    DecorateMargin.Draw();
    DecoratePadding.Draw();
    DecorateLineWidth.Draw();
    DecorateCornerRadius.Draw();
    if (!decorate_folded) EndDisabled();
    Separator();
    GroupMargin.Draw();
    GroupPadding.Draw();
    GroupLineWidth.Draw();
    GroupCornerRadius.Draw();
    Separator();
    NodeMargin.Draw();
    NodePadding.Draw();
    BoxCornerRadius.Draw();
    BinaryHorizontalGapRatio.Draw();
    WireGap.Draw();
    WireWidth.Draw();
    ArrowSize.Draw();
    InverterRadius.Draw();
}
void Style::FlowGridStyle::Params::Render() const {
    HeaderTitles.Draw();
    MinHorizontalItemWidth.Draw();
    MaxHorizontalItemWidth.Draw();
    MinVerticalItemHeight.Draw();
    MinKnobItemSize.Draw();
    AlignmentHorizontal.Draw();
    AlignmentVertical.Draw();
    Spacing();
    WidthSizingPolicy.Draw();
    TableFlags.Draw();
}
void Style::FlowGridStyle::Render() const {
    static int colors_idx = -1, graph_colors_idx = -1, graph_layout_idx = -1;
    if (Combo("Colors", &colors_idx, "Dark\0Light\0Classic\0")) q(SetFlowGridColorStyle{colors_idx});
    if (Combo("Graph colors", &graph_colors_idx, "Dark\0Light\0Classic\0Faust\0")) q(SetGraphColorStyle{graph_colors_idx});
    if (Combo("Graph layout", &graph_layout_idx, "FlowGrid\0Faust\0")) q(SetGraphLayoutStyle{graph_layout_idx});
    FlashDurationSec.Draw();

    if (BeginTabBar("")) {
        if (BeginTabItem("Faust graph", nullptr, ImGuiTabItemFlags_NoPushId)) {
            Graph.Draw();
            EndTabItem();
        }
        if (BeginTabItem("Faust params", nullptr, ImGuiTabItemFlags_NoPushId)) {
            Params.Draw();
            EndTabItem();
        }
        if (BeginTabItem(Colors.ImGuiLabel.c_str(), nullptr, ImGuiTabItemFlags_NoPushId)) {
            Colors.Draw();
        }
        EndTabBar();
    }
}

//-----------------------------------------------------------------------------
// [SECTION] Other windows
//-----------------------------------------------------------------------------

void ApplicationSettings::Render() const {
    int value = int(c.History.Index);
    if (SliderInt("History index", &value, 0, int(c.History.Size() - 1))) q(SetHistoryIndex{value});
    GestureDurationSec.Draw();
}

void Demo::ImGuiDemo::Render() const {
    ShowDemoWindow();
}
void Demo::ImPlotDemo::Render() const {
    ImPlot::ShowDemoWindow();
}
void FileDialog::Set(const FileDialogData &data, TransientStore &store) const {
    ::Set(
        {
            {Visible, true},
            {Title, data.title},
            {Filters, data.filters},
            {FilePath, data.file_path},
            {DefaultFileName, data.default_file_name},
            {SaveMode, data.save_mode},
            {MaxNumSelections, data.max_num_selections},
            {Flags, data.flags},
        },
        store
    );
}

void Demo::FileDialogDemo::Render() const {
    IGFD::ShowDemoWindow();
}

void ShowGesture(const Gesture &gesture) {
    for (Count action_index = 0; action_index < gesture.size(); action_index++) {
        const auto &[action, time] = gesture[action_index];
        JsonTree(format("{}: {}", action::GetName(action), time), json(action)[1], JsonTreeNodeFlags_None, to_string(action_index).c_str());
    }
}

void Metrics::FlowGridMetrics::Render() const {
    {
        // Active (uncompressed) gesture
        const bool widget_gesturing = UiContext.IsWidgetGesturing;
        const bool ActiveGesturePresent = !c.History.ActiveGesture.empty();
        if (ActiveGesturePresent || widget_gesturing) {
            // Gesture completion progress bar
            const auto row_item_ratio_rect = RowItemRatioRect(1 - c.History.GestureTimeRemainingSec() / s.ApplicationSettings.GestureDurationSec);
            GetWindowDrawList()->AddRectFilled(row_item_ratio_rect.Min, row_item_ratio_rect.Max, s.Style.FlowGrid.Colors[FlowGridCol_GestureIndicator]);

            const auto &ActiveGesture_title = "Active gesture"s + (ActiveGesturePresent ? " (uncompressed)" : "");
            if (TreeNodeEx(ActiveGesture_title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                if (widget_gesturing) FillRowItemBg();
                else BeginDisabled();
                Text("Widget gesture: %s", widget_gesturing ? "true" : "false");
                if (!widget_gesturing) EndDisabled();

                if (ActiveGesturePresent) ShowGesture(c.History.ActiveGesture);
                else Text("No actions yet");
                TreePop();
            }
        } else {
            BeginDisabled();
            Text("No active gesture");
            EndDisabled();
        }
    }
    Separator();
    {
        const auto &History = c.History;
        const bool no_history = History.Empty();
        if (no_history) BeginDisabled();
        if (TreeNodeEx("StoreHistory", ImGuiTreeNodeFlags_DefaultOpen, "Store event records (Count: %d, Current index: %d)", c.History.Size() - 1, History.Index)) {
            for (Count i = 1; i < History.Size(); i++) {
                if (TreeNodeEx(to_string(i).c_str(), i == History.Index ? (ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_DefaultOpen) : ImGuiTreeNodeFlags_None)) {
                    const auto &[committed, store_record, gesture] = History.Records[i];
                    BulletText("Committed: %s", format("{}\n", committed).c_str());
                    if (TreeNode("Patch")) {
                        // We compute patches as we need them rather than memoizing them.
                        const auto &patch = CreatePatch(History.Records[i - 1].Store, History.Records[i].Store);
                        for (const auto &[partial_path, op] : patch.Ops) {
                            const auto &path = patch.BasePath / partial_path;
                            if (TreeNodeEx(path.string().c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                                BulletText("Op: %s", to_string(op.Op).c_str());
                                if (op.Value) BulletText("Value: %s", to_string(*op.Value).c_str());
                                if (op.Old) BulletText("Old value: %s", to_string(*op.Old).c_str());
                                TreePop();
                            }
                        }
                        TreePop();
                    }
                    if (TreeNode("Gesture")) {
                        ShowGesture(gesture);
                        TreePop();
                    }
                    if (TreeNode("State")) {
                        JsonTree("", store_record);
                        TreePop();
                    }
                    TreePop();
                }
            }
            TreePop();
        }
        if (no_history) EndDisabled();
    }
    Separator();
    {
        // Preferences
        const bool has_RecentlyOpenedPaths = !c.Preferences.RecentlyOpenedPaths.empty();
        if (TreeNodeEx("Preferences", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (SmallButton("Clear")) c.ClearPreferences();
            SameLine();
            ShowRelativePaths.Draw();

            if (!has_RecentlyOpenedPaths) BeginDisabled();
            if (TreeNodeEx("Recently opened paths", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (const auto &recently_opened_path : c.Preferences.RecentlyOpenedPaths) {
                    BulletText("%s", (ShowRelativePaths ? fs::relative(recently_opened_path) : recently_opened_path).c_str());
                }
                TreePop();
            }
            if (!has_RecentlyOpenedPaths) EndDisabled();

            TreePop();
        }
    }
    Separator();
    {
        // Various internals
        Text("Action variant size: %lu bytes", sizeof(Action));
        Text("Primitive variant size: %lu bytes", sizeof(Primitive));
        SameLine();
        HelpMarker("All actions are internally stored in an `std::variant`, which must be large enough to hold its largest type. "
                   "Thus, it's important to keep action data small.");
    }
}
void Metrics::ImGuiMetrics::Render() const { ShowMetricsWindow(); }
void Metrics::ImPlotMetrics::Render() const { ImPlot::ShowMetricsWindow(); }

void DebugLog::Render() const {
    ShowDebugLogWindow();
}
void StackTool::Render() const {
    ShowStackToolWindow();
}

void FaustState::FaustLog::Render() const {
    PushStyleColor(ImGuiCol_Text, {1, 0, 0, 1});
    Error.Draw();
    PopStyleColor();
}

void FaustState::Render() const {
    Editor.Draw();
    Graph.Draw();
    Params.Draw();
    Log.Draw();
}
