#include "StateJson.h"

#include <fstream>
#include <range/v3/view/concat.hpp>

#include "imgui_memory_editor.h"
#include "implot_internal.h"

#include "FileDialog/FileDialogDemo.h"

using namespace ImGui;
using namespace fg;
using namespace action;

//-----------------------------------------------------------------------------
// [SECTION] Fields
//-----------------------------------------------------------------------------

// Currently, `Draw` is not used for anything except wrapping around `Render`.
// Fields don't wrap their `Render` with a push/pop-id, ImGui widgets all push the provided label to the ID stack.
void Drawable::Draw() const {
    //    PushID(ImGuiLabel.c_str());
    Render();
    //    PopID();
}

namespace Field {
Base::Base(StateMember *parent, string_view id, string_view name_help, const Primitive &value) : UIStateMember(parent, id, name_help) {
    c.InitStore.set(Path, value);
}
Primitive Base::Get() const { return AppStore.at(Path); }
Primitive Base::GetInitial() const { return c.InitStore.at(Path); }

Bool::operator bool() const { return std::get<bool>(Get()); }
Int::operator int() const { return std::get<int>(Get()); }
UInt::operator U32() const { return std::get<U32>(Get()); }
Float::operator float() const {
    const Primitive &value = Get();
    if (std::holds_alternative<int>(value)) return float(std::get<int>(value));
    return std::get<float>(value);
}
String::operator string() const { return std::get<string>(Get()); }
bool String::operator==(const string &v) const { return string(*this) == v; }
String::operator bool() const { return !string(*this).empty(); }
Enum::operator int() const { return std::get<int>(Get()); }
Flags::operator int() const { return std::get<int>(Get()); }
} // namespace Field

template<typename T>
T Vector<T>::operator[](Count index) const { return std::get<T>(AppStore.at(Path / to_string(index))); };
template<typename T>
Count Vector<T>::size(const Store &store) const {
    Count i = 0;
    while (store.count(Path / to_string(i++))) {}
    return i - 1;
}

// Transient
template<typename T>
void Vector<T>::Set(Count index, const T &value, TransientStore &store) const { store.set(Path / to_string(index), value); }
template<typename T>
void Vector<T>::Set(const vector<T> &values, TransientStore &store) const {
    ::Set(views::ints(0, int(values.size())) | transform([&](const int i) { return StoreEntry(Path / to_string(i), values[i]); }) | to<vector>, store);
    truncate(values.size(), store);
}
template<typename T>
Store Vector<T>::Set(const vector<pair<int, T>> &values, const Store &store) const {
    auto transient = store.transient();
    Set(values, transient);
    return transient.persistent();
}

// Persistent
template<typename T>
Store Vector<T>::Set(Count index, const T &value, const Store &store) const { return ::Set(Path / index, value, store); }
template<typename T>
Store Vector<T>::Set(const vector<T> &values, const Store &store) const {
    if (values.empty()) return store;

    auto transient = store.transient();
    Set(values, transient);
    return transient.persistent();
}
template<typename T>
void Vector<T>::Set(const vector<pair<int, T>> &values, TransientStore &store) const {
    for (const auto &[index, value] : values) store.set(Path / to_string(index), value);
}

template<typename T>
void Vector<T>::truncate(const Count length, TransientStore &store) const {
    Count i = length;
    while (store.count(Path / to_string(i))) store.erase(Path / to_string(i++));
}

template<typename T>
T Vector2D<T>::at(Count i, Count j, const Store &store) const { return std::get<T>(store.at(Path / to_string(i) / to_string(j))); };
template<typename T>
Count Vector2D<T>::size(const TransientStore &store) const {
    Count i = 0;
    while (store.count(Path / i++ / 0).to_string()) {}
    return i - 1;
}
template<typename T>
Store Vector2D<T>::Set(Count i, Count j, const T &value, const Store &store) const { return store.set(Path / to_string(i) / to_string(j), value); }
template<typename T>
void Vector2D<T>::Set(Count i, Count j, const T &value, TransientStore &store) const { store.set(Path / to_string(i) / to_string(j), value); }
template<typename T>
void Vector2D<T>::truncate(const Count length, TransientStore &store) const {
    Count i = length;
    while (store.count(Path / to_string(i) / "0")) truncate(i++, 0, store);
}
template<typename T>
void Vector2D<T>::truncate(const Count i, const Count length, TransientStore &store) const {
    Count j = length;
    while (store.count(Path / to_string(i) / to_string(j))) store.erase(Path / to_string(i) / to_string(j++));
}

//-----------------------------------------------------------------------------
// [SECTION] Actions
//-----------------------------------------------------------------------------

PatchOps Merge(const PatchOps &a, const PatchOps &b) {
    PatchOps merged = a;
    for (const auto &[path, op] : b) {
        if (merged.contains(path)) {
            const auto &old_op = merged.at(path);
            // Strictly, two consecutive patches that both add or both remove the same key should throw an exception,
            // but I'm being lax here to allow for merging multiple patches by only looking at neighbors.
            // For example, if the first patch removes a path, and the second one adds the same path,
            // we can't know from only looking at the pair whether the added value was the same as it was before the remove
            // (in which case it should just be `Remove` during merge) or if it was different (in which case the merged action should be a `Replace`).
            if (old_op.Op == AddOp) {
                if (op.Op == RemoveOp || ((op.Op == AddOp || op.Op == ReplaceOp) && old_op.Value == op.Value)) merged.erase(path); // Cancel out
                else merged[path] = {AddOp, op.Value, {}};
            } else if (old_op.Op == RemoveOp) {
                if (op.Op == AddOp || op.Op == ReplaceOp) {
                    if (old_op.Value == op.Value) merged.erase(path); // Cancel out
                    else merged[path] = {ReplaceOp, op.Value, old_op.Old};
                } else {
                    merged[path] = {RemoveOp, {}, old_op.Old};
                }
            } else if (old_op.Op == ReplaceOp) {
                if (op.Op == AddOp || op.Op == ReplaceOp) merged[path] = {ReplaceOp, op.Value, old_op.Old};
                else merged[path] = {RemoveOp, {}, old_op.Old};
            }
        } else {
            merged[path] = op;
        }
    }

    return merged;
}

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
std::variant<StateAction, bool> Merge(const StateAction &a, const StateAction &b) {
    const ID a_id = GetId(a);
    const ID b_id = GetId(b);

    switch (a_id) {
        case id<OpenFileDialog>:
        case id<CloseFileDialog>:
        case id<ShowOpenProjectDialog>:
        case id<ShowSaveProjectDialog>:
        case id<CloseApplication>:
        case id<SetImGuiColorStyle>:
        case id<SetImPlotColorStyle>:
        case id<SetFlowGridColorStyle>:
        case id<SetDiagramColorStyle>:
        case id<SetDiagramLayoutStyle>:
        case id<ShowOpenFaustFileDialog>:
        case id<ShowSaveFaustFileDialog>: {
            if (a_id == b_id) return b;
            return false;
        }
        case id<OpenFaustFile>:
        case id<SetValue>: {
            if (a_id == b_id && std::get<SetValue>(a).path == std::get<SetValue>(b).path) return b;
            return false;
        }
        case id<SetValues>: {
            if (a_id == b_id) return SetValues{views::concat(std::get<SetValues>(a).values, std::get<SetValues>(b).values) | to<std::vector>};
            return false;
        }
        case id<ToggleValue>: return a_id == b_id && std::get<ToggleValue>(a).path == std::get<ToggleValue>(b).path;
        case id<ApplyPatch>: {
            if (a_id == b_id) {
                const auto &_a = std::get<ApplyPatch>(a);
                const auto &_b = std::get<ApplyPatch>(b);
                // Keep patch actions affecting different base state-paths separate,
                // since actions affecting different state bases are likely semantically different.
                const auto &ops = Merge(_a.patch.Ops, _b.patch.Ops);
                if (ops.empty()) return true;
                if (_a.patch.BasePath == _b.patch.BasePath) return ApplyPatch{ops, _b.patch.BasePath};
                return false;
            }
            return false;
        }
        default: return false;
    }
}

Gesture action::MergeGesture(const Gesture &gesture) {
    Gesture merged_gesture; // Mutable return value
    // `active` keeps track of which action we're merging into.
    // It's either an action in `gesture` or the result of merging 2+ of its consecutive members.
    std::optional<const StateActionMoment> active;
    for (Count i = 0; i < gesture.size(); i++) {
        if (!active) active.emplace(gesture[i]);
        const auto &a = *active;
        const auto &b = gesture[i + 1];
        std::variant<StateAction, bool> merge_result = Merge(a.first, b.first);
        Match(
            merge_result,
            [&](const bool cancel_out) {
                if (cancel_out) i++; // The two actions (`a` and `b`) cancel out, so we add neither. (Skip over `b` entirely.)
                else merged_gesture.emplace_back(a); //
                active.reset(); // No merge in either case. Move on to try compressing the next action.
            },
            [&](const StateAction &merged_action) {
                active.emplace(merged_action, b.second); // The two actions were merged. Keep track of it but don't add it yet - maybe we can merge more actions into it.
            },
        );
    }
    if (active) merged_gesture.emplace_back(*active);

    return merged_gesture;
}

// Helper to display a (?) mark which shows a tooltip when hovered. From `imgui_demo.cpp`.
void StateMember::HelpMarker(const bool after) const {
    if (Help.empty()) return;

    if (after) SameLine();
    ::HelpMarker(Help.c_str());
    if (!after) SameLine();
}

void Bool::Toggle() const { q(ToggleValue{Path}); }

void Field::Bool::Render() const {
    bool value = *this;
    if (Checkbox(ImGuiLabel.c_str(), &value)) Toggle();
    HelpMarker();
}
bool Field::Bool::CheckedDraw() const {
    bool value = *this;
    bool toggled = Checkbox(ImGuiLabel.c_str(), &value);
    if (toggled) Toggle();
    HelpMarker();
    return toggled;
}
void Field::Bool::MenuItem() const {
    const bool value = *this;
    HelpMarker(false);
    if (ImGui::MenuItem(ImGuiLabel.c_str(), nullptr, value)) Toggle();
}

void Field::UInt::Render() const {
    U32 value = *this;
    const bool edited = SliderScalar(ImGuiLabel.c_str(), ImGuiDataType_S32, &value, &min, &max, "%d");
    UiContext.WidgetGestured();
    if (edited) q(SetValue{Path, value});
    HelpMarker();
}

void Field::Int::Render() const {
    int value = *this;
    const bool edited = SliderInt(ImGuiLabel.c_str(), &value, min, max, "%d", ImGuiSliderFlags_None);
    UiContext.WidgetGestured();
    if (edited) q(SetValue{Path, value});
    HelpMarker();
}
void Field::Int::Render(const vector<int> &options) const {
    const int value = *this;
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

void Field::Float::Render() const {
    float value = *this;
    const bool edited = DragSpeed > 0 ? DragFloat(ImGuiLabel.c_str(), &value, DragSpeed, Min, Max, Format, Flags) : SliderFloat(ImGuiLabel.c_str(), &value, Min, Max, Format, Flags);
    UiContext.WidgetGestured();
    if (edited) q(SetValue{Path, value});
    HelpMarker();
}

void Field::Enum::Render() const {
    Render(views::ints(0, int(Names.size())) | to<vector<int>>); // todo if I stick with this pattern, cache.
}
void Field::Enum::Render(const vector<int> &options) const {
    const int value = *this;
    if (BeginCombo(ImGuiLabel.c_str(), Names[value].c_str())) {
        for (int option : options) {
            const bool is_selected = option == value;
            const auto &name = Names[option];
            if (Selectable(name.c_str(), is_selected)) q(SetValue{Path, option});
            if (is_selected) SetItemDefaultFocus();
        }
        EndCombo();
    }
    HelpMarker();
}
void Field::Enum::MenuItem() const {
    const int value = *this;
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

void Field::Flags::Render() const {
    const int value = *this;
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
void Field::Flags::MenuItem() const {
    const int value = *this;
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

void Field::String::Render() const {
    const string value = *this;
    TextUnformatted(value.c_str());
}
void Field::String::Render(const vector<string> &options) const {
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

//-----------------------------------------------------------------------------
// [SECTION] Helpers
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

void Vec2::Render(ImGuiSliderFlags flags) const {
    ImVec2 values = *this;
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&values, min, max, fmt, flags);
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
    const bool edited = SliderFloat2(ImGuiLabel.c_str(), (float *)&values, min, max, fmt, flags);
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

//-----------------------------------------------------------------------------
// [SECTION] Window/tabs methods
//-----------------------------------------------------------------------------

Window::Window(StateMember *parent, string_view path_segment, string_view name_help, const bool visible)
    : StateMember(parent, path_segment, name_help) {
    Set(Visible, visible, c.InitStore);
}
Window::Window(StateMember *parent, string_view path_segment, string_view name_help, Menu menu)
    : StateMember(parent, path_segment, name_help), WindowMenu{std::move(menu)} {
    Set(Visible, true, c.InitStore);
}

void Window::Draw(ImGuiWindowFlags flags) const {
    if (!Visible) return;

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
            if (const auto *ui_child = reinterpret_cast<const UIStateMember *>(child)) {
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
    if (is_menu_bar ? BeginMenuBar() : BeginMenu(Label.c_str())) {
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
        if (is_menu_bar) EndMenuBar();
        else EndMenu();
    }
}

void Info::Render() const {
    const auto hovered_id = GetHoveredID();
    if (hovered_id && StateMember::WithId.contains(hovered_id)) {
        const auto *member = StateMember::WithId.at(hovered_id);
        const string help = member->Help.empty() ? format("No info available for {}.", member->Name) : member->Help;
        PushTextWrapPos(0);
        TextUnformatted(help.c_str());
    }
}

void State::UIProcess::Render() const {}
void State::Render() const {
    if (BeginMainMenuBar()) {
        if (BeginMenu("File")) {
            ActionMenuItem(OpenEmptyProject{});
            ActionMenuItem(ShowOpenProjectDialog{});
            if (BeginMenu("Open recent project", !c.Preferences.RecentlyOpenedPaths.empty())) {
                for (const auto &recently_opened_path : c.Preferences.RecentlyOpenedPaths) {
                    if (ImGui::MenuItem(recently_opened_path.filename().c_str())) q(OpenProject{recently_opened_path});
                }
                EndMenu();
            }
            ActionMenuItem(OpenDefaultProject{});

            ActionMenuItem(SaveCurrentProject{});
            ActionMenuItem(ShowSaveProjectDialog{});
            ActionMenuItem(SaveDefaultProject{});
            EndMenu();
        }
        if (BeginMenu("Edit")) {
            ActionMenuItem(Undo{});
            ActionMenuItem(Redo{});
            EndMenu();
        }
        if (BeginMenu("Windows")) {
            if (BeginMenu("Debug")) {
                DebugLog.MenuItem();
                StackTool.MenuItem();
                StateViewer.MenuItem();
                StatePathUpdateFrequency.MenuItem();
                StateMemoryEditor.MenuItem();
                ProjectPreview.MenuItem();
                EndMenu();
            }
            if (BeginMenu("Audio")) {
                Audio.MenuItem();
                if (BeginMenu("Faust")) {
                    Audio.Faust.Editor.MenuItem();
                    Audio.Faust.Diagram.MenuItem();
                    Audio.Faust.Params.MenuItem();
                    Audio.Faust.Log.MenuItem();
                    EndMenu();
                }
                EndMenu();
            }
            Metrics.MenuItem();
            Style.MenuItem();
            Demo.MenuItem();
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

    ApplicationSettings.Draw();
    Audio.Draw();

    Audio.Faust.Editor.Draw(ImGuiWindowFlags_MenuBar);
    Audio.Faust.Diagram.Draw(ImGuiWindowFlags_MenuBar);
    Audio.Faust.Params.Draw();
    Audio.Faust.Log.Draw();

    DebugLog.Draw();
    StackTool.Draw();
    StateViewer.Draw(ImGuiWindowFlags_MenuBar);
    StatePathUpdateFrequency.Draw();
    StateMemoryEditor.Draw(ImGuiWindowFlags_NoScrollbar);
    ProjectPreview.Draw();

    Metrics.Draw();
    Style.Draw();
    Demo.Draw(ImGuiWindowFlags_MenuBar);
    FileDialog.Draw();
    Info.Draw();
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
    for (Count i = 0; i < size; i++) {
        const auto &ds = dss[int(i)];
        NodeId.Set(i, ds.NodeId, store);
        ParentNodeId.Set(i, ds.ParentNodeId, store);
        ParentWindowId.Set(i, ds.ParentWindowId, store);
        SelectedTabId.Set(i, ds.SelectedTabId, store);
        SplitAxis.Set(i, ds.SplitAxis, store);
        Depth.Set(i, ds.Depth, store);
        Flags.Set(i, int(ds.Flags), store);
        Pos.Set(i, PackImVec2ih(ds.Pos), store);
        Size.Set(i, PackImVec2ih(ds.Size), store);
        SizeRef.Set(i, PackImVec2ih(ds.SizeRef), store);
    }
    NodeId.truncate(size, store);
    ParentNodeId.truncate(size, store);
    ParentWindowId.truncate(size, store);
    SelectedTabId.truncate(size, store);
    SplitAxis.truncate(size, store);
    Depth.truncate(size, store);
    Flags.truncate(size, store);
    Pos.truncate(size, store);
    Size.truncate(size, store);
    SizeRef.truncate(size, store);
}
void DockNodeSettings::Apply(ImGuiContext *ctx) const {
    // Assumes `DockSettingsHandler_ClearAll` has already been called.
    for (Count i = 0; i < NodeId.size(); i++) {
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
    Count i = 0;
    for (auto *ws = wss.begin(); ws != nullptr; ws = wss.next_chunk(ws)) {
        ID.Set(i, ws->ID, store);
        ClassId.Set(i, ws->DockId, store);
        ViewportId.Set(i, ws->ViewportId, store);
        DockId.Set(i, ws->DockId, store);
        DockOrder.Set(i, ws->DockOrder, store);
        Pos.Set(i, PackImVec2ih(ws->Pos), store);
        Size.Set(i, PackImVec2ih(ws->Size), store);
        ViewportPos.Set(i, PackImVec2ih(ws->ViewportPos), store);
        Collapsed.Set(i, ws->Collapsed, store);
        i++;
    }
    ID.truncate(i, store);
    ClassId.truncate(i, store);
    ViewportId.truncate(i, store);
    DockId.truncate(i, store);
    DockOrder.truncate(i, store);
    Pos.truncate(i, store);
    Size.truncate(i, store);
    ViewportPos.truncate(i, store);
    Collapsed.truncate(i, store);
}
// See `imgui.cpp::ApplyWindowSettings`
void WindowSettings::Apply(ImGuiContext *) const {
    const auto *main_viewport = GetMainViewport();
    for (Count i = 0; i < ID.size(); i++) {
        const auto id = ID[i];
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
    Count i = 0;
    for (auto *ts = tss.begin(); ts != nullptr; ts = tss.next_chunk(ts)) {
        auto columns_count = ts->ColumnsCount;

        ID.Set(i, ts->ID, store);
        SaveFlags.Set(i, ts->SaveFlags, store);
        RefScale.Set(i, ts->RefScale, store);
        ColumnsCount.Set(i, columns_count, store);
        ColumnsCountMax.Set(i, ts->ColumnsCountMax, store);
        WantApply.Set(i, ts->WantApply, store);
        for (int column_index = 0; column_index < columns_count; column_index++) {
            const auto &cs = ts->GetColumnSettings()[column_index];
            Columns.WidthOrWeight.Set(i, column_index, cs.WidthOrWeight, store);
            Columns.UserID.Set(i, column_index, cs.UserID, store);
            Columns.Index.Set(i, column_index, cs.Index, store);
            Columns.DisplayOrder.Set(i, column_index, cs.DisplayOrder, store);
            Columns.SortOrder.Set(i, column_index, cs.SortOrder, store);
            Columns.SortDirection.Set(i, column_index, cs.SortDirection, store);
            Columns.IsEnabled.Set(i, column_index, cs.IsEnabled, store);
            Columns.IsStretch.Set(i, column_index, cs.IsStretch, store);
        }
        Columns.WidthOrWeight.truncate(i, columns_count, store);
        Columns.UserID.truncate(i, columns_count, store);
        Columns.Index.truncate(i, columns_count, store);
        Columns.DisplayOrder.truncate(i, columns_count, store);
        Columns.SortOrder.truncate(i, columns_count, store);
        Columns.SortDirection.truncate(i, columns_count, store);
        Columns.IsEnabled.truncate(i, columns_count, store);
        Columns.IsStretch.truncate(i, columns_count, store);
        i++;
    }
    ID.truncate(i, store);
    SaveFlags.truncate(i, store);
    RefScale.truncate(i, store);
    ColumnsCount.truncate(i, store);
    ColumnsCountMax.truncate(i, store);
    WantApply.truncate(i, store);
    Columns.WidthOrWeight.truncate(i, store);
    Columns.UserID.truncate(i, store);
    Columns.Index.truncate(i, store);
    Columns.DisplayOrder.truncate(i, store);
    Columns.SortOrder.truncate(i, store);
    Columns.SortDirection.truncate(i, store);
    Columns.IsEnabled.truncate(i, store);
    Columns.IsStretch.truncate(i, store);
}
// Adapted from `imgui_tables.cpp::TableLoadSettings`
void TableSettings::Apply(ImGuiContext *) const {
    for (Count i = 0; i < ID.size(); i++) {
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
            int column_n = Columns.Index.at(i, j);
            if (column_n < 0 || column_n >= table->ColumnsCount) continue;

            ImGuiTableColumn *column = &table->Columns[column_n];
            if (ImGuiTableFlags(SaveFlags[i]) & ImGuiTableFlags_Resizable) {
                float width_or_weight = Columns.WidthOrWeight.at(i, j);
                if (Columns.IsStretch.at(i, j)) column->StretchWeight = width_or_weight;
                else column->WidthRequest = width_or_weight;
                column->AutoFitQueue = 0x00;
            }
            column->DisplayOrder = ImGuiTableFlags(SaveFlags[i]) & ImGuiTableFlags_Reorderable ? ImGuiTableColumnIdx(Columns.DisplayOrder.at(i, j)) : (ImGuiTableColumnIdx)column_n;
            display_order_mask |= (ImU64)1 << column->DisplayOrder;
            column->IsUserEnabled = column->IsUserEnabledNextFrame = Columns.IsEnabled.at(i, j);
            column->SortOrder = ImGuiTableColumnIdx(Columns.SortOrder.at(i, j));
            column->SortDirection = Columns.SortDirection.at(i, j);
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
        (is_imgui_color        ? s.Style.ImGui.Colors.GetName(array_index) :
             is_implot_color   ? s.Style.ImPlot.Colors.GetName(array_index) :
             is_flowgrid_color ? s.Style.FlowGrid.Colors.GetName(array_index) :
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
Style::FlowGridStyle::Diagram::Diagram(StateMember *parent, string_view path_segment, string_view name_help)
    : UIStateMember(parent, path_segment, name_help) {
    ColorsDark(c.InitStore);
    LayoutFlowGrid(c.InitStore);
}

void Style::ImGuiStyle::ColorsDark(TransientStore &store) const {
    vector<ImVec4> dst(ImGuiCol_COUNT);
    StyleColorsDark(&dst[0]);
    Colors.Set(dst, store);
}
void Style::ImGuiStyle::ColorsLight(TransientStore &store) const {
    vector<ImVec4> dst(ImGuiCol_COUNT);
    StyleColorsLight(&dst[0]);
    Colors.Set(dst, store);
}
void Style::ImGuiStyle::ColorsClassic(TransientStore &store) const {
    vector<ImVec4> dst(ImGuiCol_COUNT);
    StyleColorsClassic(&dst[0]);
    Colors.Set(dst, store);
}

void Style::ImPlotStyle::ColorsAuto(TransientStore &store) const {
    vector<ImVec4> dst(ImPlotCol_COUNT);
    ImPlot::StyleColorsAuto(&dst[0]);
    Colors.Set(dst, store);
    Set(MinorAlpha, 0.25f, store);
}
void Style::ImPlotStyle::ColorsDark(TransientStore &store) const {
    vector<ImVec4> dst(ImPlotCol_COUNT);
    ImPlot::StyleColorsDark(&dst[0]);
    Colors.Set(dst, store);
    Set(MinorAlpha, 0.25f, store);
}
void Style::ImPlotStyle::ColorsLight(TransientStore &store) const {
    vector<ImVec4> dst(ImPlotCol_COUNT);
    ImPlot::StyleColorsLight(&dst[0]);
    Colors.Set(dst, store);
    Set(MinorAlpha, 1, store);
}
void Style::ImPlotStyle::ColorsClassic(TransientStore &store) const {
    vector<ImVec4> dst(ImPlotCol_COUNT);
    ImPlot::StyleColorsClassic(&dst[0]);
    Colors.Set(dst, store);
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

void Style::FlowGridStyle::Diagram::ColorsDark(TransientStore &store) const {
    Colors.Set(
        {
            {FlowGridDiagramCol_Bg, {0.06, 0.06, 0.06, 0.94}},
            {FlowGridDiagramCol_Text, {1, 1, 1, 1}},
            {FlowGridDiagramCol_DecorateStroke, {0.43, 0.43, 0.5, 0.5}},
            {FlowGridDiagramCol_GroupStroke, {0.43, 0.43, 0.5, 0.5}},
            {FlowGridDiagramCol_Line, {0.61, 0.61, 0.61, 1}},
            {FlowGridDiagramCol_Link, {0.26, 0.59, 0.98, 0.4}},
            {FlowGridDiagramCol_Inverter, {1, 1, 1, 1}},
            {FlowGridDiagramCol_OrientationMark, {1, 1, 1, 1}},
            // Box fills
            {FlowGridDiagramCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridDiagramCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridDiagramCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridDiagramCol_Number, {0.96, 0.28, 0, 1}},
        },
        store
    );
}
void Style::FlowGridStyle::Diagram::ColorsClassic(TransientStore &store) const {
    Colors.Set(
        {
            {FlowGridDiagramCol_Bg, {0, 0, 0, 0.85}},
            {FlowGridDiagramCol_Text, {0.9, 0.9, 0.9, 1}},
            {FlowGridDiagramCol_DecorateStroke, {0.5, 0.5, 0.5, 0.5}},
            {FlowGridDiagramCol_GroupStroke, {0.5, 0.5, 0.5, 0.5}},
            {FlowGridDiagramCol_Line, {1, 1, 1, 1}},
            {FlowGridDiagramCol_Link, {0.35, 0.4, 0.61, 0.62}},
            {FlowGridDiagramCol_Inverter, {0.9, 0.9, 0.9, 1}},
            {FlowGridDiagramCol_OrientationMark, {0.9, 0.9, 0.9, 1}},
            // Box fills
            {FlowGridDiagramCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridDiagramCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridDiagramCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridDiagramCol_Number, {0.96, 0.28, 0, 1}},
        },
        store
    );
}
void Style::FlowGridStyle::Diagram::ColorsLight(TransientStore &store) const {
    Colors.Set(
        {
            {FlowGridDiagramCol_Bg, {0.94, 0.94, 0.94, 1}},
            {FlowGridDiagramCol_Text, {0, 0, 0, 1}},
            {FlowGridDiagramCol_DecorateStroke, {0, 0, 0, 0.3}},
            {FlowGridDiagramCol_GroupStroke, {0, 0, 0, 0.3}},
            {FlowGridDiagramCol_Line, {0.39, 0.39, 0.39, 1}},
            {FlowGridDiagramCol_Link, {0.26, 0.59, 0.98, 0.4}},
            {FlowGridDiagramCol_Inverter, {0, 0, 0, 1}},
            {FlowGridDiagramCol_OrientationMark, {0, 0, 0, 1}},
            // Box fills
            {FlowGridDiagramCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridDiagramCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridDiagramCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridDiagramCol_Number, {0.96, 0.28, 0, 1}},
        },
        store
    );
}
void Style::FlowGridStyle::Diagram::ColorsFaust(TransientStore &store) const {
    Colors.Set(
        {
            {FlowGridDiagramCol_Bg, {1, 1, 1, 1}},
            {FlowGridDiagramCol_Text, {1, 1, 1, 1}},
            {FlowGridDiagramCol_DecorateStroke, {0.2, 0.2, 0.2, 1}},
            {FlowGridDiagramCol_GroupStroke, {0.2, 0.2, 0.2, 1}},
            {FlowGridDiagramCol_Line, {0, 0, 0, 1}},
            {FlowGridDiagramCol_Link, {0, 0.2, 0.4, 1}},
            {FlowGridDiagramCol_Inverter, {0, 0, 0, 1}},
            {FlowGridDiagramCol_OrientationMark, {0, 0, 0, 1}},
            // Box fills
            {FlowGridDiagramCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridDiagramCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridDiagramCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridDiagramCol_Number, {0.96, 0.28, 0, 1}},
        },
        store
    );
}

void Style::FlowGridStyle::Diagram::LayoutFlowGrid(TransientStore &store) const {
    Set(DefaultLayoutEntries, store);
}
void Style::FlowGridStyle::Diagram::LayoutFaust(TransientStore &store) const {
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

void Colors::Draw() const {
    if (BeginTabItem(ImGuiLabel.c_str(), nullptr, ImGuiTabItemFlags_NoPushId)) {
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
        for (Count i = 0; i < size(); i++) {
            const U32 value = (*this)[i];
            const bool is_auto = AllowAuto && value == AutoColor;
            const U32 mapped_value = is_auto ? ColorConvertFloat4ToU32(ImPlot::GetAutoColor(int(i))) : value;

            const string &name = GetName(i);
            if (!filter.PassFilter(name.c_str())) continue;

            PushID(int(i));
            // todo use auto for FG colors (link to ImGui colors)
            if (AllowAuto) {
                if (!is_auto) PushStyleVar(ImGuiStyleVar_Alpha, 0.25);
                if (Button("Auto")) q(SetValue{Path / to_string(i), is_auto ? mapped_value : AutoColor});
                if (!is_auto) PopStyleVar();
                SameLine();
            }
            auto mutable_value = ColorConvertU32ToFloat4(mapped_value);
            if (is_auto) BeginDisabled();
            const bool item_changed = ColorEdit4(PathLabel(Path / to_string(i)).c_str(), (float *)&mutable_value, alpha_flags | ImGuiColorEditFlags_AlphaBar | (AllowAuto ? ImGuiColorEditFlags_AlphaPreviewHalf : 0));
            UiContext.WidgetGestured();
            if (is_auto) EndDisabled();

            SameLine(0, style.ItemInnerSpacing.x);
            TextUnformatted(name.c_str());
            PopID();

            if (item_changed) q(SetValue{Path / to_string(i), ColorConvertFloat4ToU32(mutable_value)});
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
                    const float RAD_MIN = 5;
                    const float RAD_MAX = 70;
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
        Colors.Draw();
        EndTabBar();
    }
}

void Style::FlowGridStyle::Diagram::Render() const {
    FoldComplexity.Draw();
    const bool scale_fill = ScaleFillHeight;
    ScaleFillHeight.Draw();
    if (scale_fill) BeginDisabled();
    Scale.Draw();
    if (scale_fill) {
        SameLine();
        TextUnformatted(format("Uncheck '{}' to manually edit diagram scale.", ScaleFillHeight.Name).c_str());
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
    static int colors_idx = -1, diagram_colors_idx = -1, diagram_layout_idx = -1;
    if (Combo("Colors", &colors_idx, "Dark\0Light\0Classic\0")) q(SetFlowGridColorStyle{colors_idx});
    if (Combo("Diagram colors", &diagram_colors_idx, "Dark\0Light\0Classic\0Faust\0")) q(SetDiagramColorStyle{diagram_colors_idx});
    if (Combo("Diagram layout", &diagram_layout_idx, "FlowGrid\0Faust\0")) q(SetDiagramLayoutStyle{diagram_layout_idx});
    FlashDurationSec.Draw();

    if (BeginTabBar("")) {
        if (BeginTabItem("Faust diagram", nullptr, ImGuiTabItemFlags_NoPushId)) {
            Diagram.Draw();
            EndTabItem();
        }
        if (BeginTabItem("Faust params", nullptr, ImGuiTabItemFlags_NoPushId)) {
            Params.Draw();
            EndTabItem();
        }
        Colors.Draw();
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

const vector<int> Audio::PrioritizedDefaultSampleRates = {48000, 44100, 96000};
const vector<Audio::IoFormat> Audio::PrioritizedDefaultFormats = {
    IoFormat_Float64NE,
    IoFormat_Float32NE,
    IoFormat_S32NE,
    IoFormat_S16NE,
    IoFormat_Invalid,
};

void Demo::ImGuiDemo::Render() const {
    ShowDemoWindow();
}
void Demo::ImPlotDemo::Render() const {
    ImPlot::ShowDemoWindow();
}
void FileDialog::Set(const FileDialogData &data, TransientStore &store) const {
    ::Set(
        {
            {Title, data.title},
            {Filters, data.filters},
            {FilePath, data.file_path},
            {DefaultFileName, data.default_file_name},
            {SaveMode, data.save_mode},
            {MaxNumSelections, data.max_num_selections},
            {Flags, data.flags},
            {Visible, true},
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
