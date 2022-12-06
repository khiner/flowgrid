#include "StateJson.h"

#include <fstream>
#include <range/v3/view/concat.hpp>

#include "implot_internal.h"
#include "imgui_memory_editor.h"

#include "FileDialog/FileDialogDemo.h"

using namespace ImGui;
using namespace fg;
using namespace action;

//-----------------------------------------------------------------------------
// [SECTION] Fields
//-----------------------------------------------------------------------------

namespace Field {
Primitive Base::Get() const { return store.at(Path); }
Primitive Base::GetInitial() const { return c.CtorStore.at(Path); }

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
}

template<typename T>
T Vector<T>::operator[](Count index) const { return std::get<T>(store.at(Path / to_string(index))); };
template<typename T>
Count Vector<T>::size(const Store &_store) const {
    Count i = 0;
    while (_store.count(Path / to_string(i++))) {}
    return i - 1;
}

// Transient
template<typename T>
void Vector<T>::Set(Count index, const T &value, TransientStore &_store) const { _store.set(Path / to_string(index), value); }
template<typename T>
void Vector<T>::Set(const vector<T> &values, TransientStore &_store) const {
    ::Set(views::ints(0, int(values.size())) | transform([&](const int i) { return StoreEntry(Path / to_string(i), values[i]); }) | to<vector>, _store);
    truncate(values.size(), _store);
}
template<typename T>
Store Vector<T>::Set(const vector<std::pair<int, T>> &values, const Store &_store) const {
    auto transient = _store.transient();
    Set(values, transient);
    return transient.persistent();
}

// Persistent
template<typename T>
Store Vector<T>::Set(Count index, const T &value, const Store &_store) const { return ::Set(Path / index, value, _store); }
template<typename T>
Store Vector<T>::Set(const vector<T> &values, const Store &_store) const {
    if (values.empty()) return _store;

    auto transient = _store.transient();
    Set(values, transient);
    return transient.persistent();
}
template<typename T>
void Vector<T>::Set(const vector<std::pair<int, T>> &values, TransientStore &_store) const {
    for (const auto &[index, value]: values) _store.set(Path / to_string(index), value);
}

template<typename T>
void Vector<T>::truncate(const Count length, TransientStore &_store) const {
    Count i = length;
    while (_store.count(Path / to_string(i))) _store.erase(Path / to_string(i++));
}

template<typename T>
T Vector2D<T>::at(Count i, Count j, const Store &_store) const { return std::get<T>(_store.at(Path / to_string(i) / to_string(j))); };
template<typename T>
Count Vector2D<T>::size(const TransientStore &_store) const {
    Count i = 0;
    while (_store.count(Path / i++ / 0).to_string()) {}
    return i - 1;
}
template<typename T>
Store Vector2D<T>::Set(Count i, Count j, const T &value, const Store &_store) const { return _store.set(Path / to_string(i) / to_string(j), value); }
template<typename T>
void Vector2D<T>::Set(Count i, Count j, const T &value, TransientStore &_store) const { _store.set(Path / to_string(i) / to_string(j), value); }
template<typename T>
void Vector2D<T>::truncate(const Count length, TransientStore &_store) const {
    Count i = length;
    while (_store.count(Path / to_string(i) / "0")) truncate(i++, 0, _store);
}
template<typename T>
void Vector2D<T>::truncate(const Count i, const Count length, TransientStore &_store) const {
    Count j = length;
    while (_store.count(Path / to_string(i) / to_string(j))) _store.erase(Path / to_string(i) / to_string(j++));
}

//-----------------------------------------------------------------------------
// [SECTION] Actions
//-----------------------------------------------------------------------------

PatchOps Merge(const PatchOps &a, const PatchOps &b) {
    PatchOps merged = a;
    for (const auto &[path, op]: b) {
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
        case id<open_file_dialog>:
        case id<close_file_dialog>:
        case id<show_open_project_dialog>:
        case id<show_save_project_dialog>:
        case id<close_application>:
        case id<set_imgui_color_style>:
        case id<set_implot_color_style>:
        case id<set_flowgrid_color_style>:
        case id<set_flowgrid_diagram_color_style>:
        case id<set_flowgrid_diagram_layout_style>:
        case id<show_open_faust_file_dialog>:
        case id<show_save_faust_file_dialog>: {
            if (a_id == b_id) return b;
            return false;
        }
        case id<open_faust_file>:
        case id<set_value>: {
            if (a_id == b_id && std::get<set_value>(a).path == std::get<set_value>(b).path) return b;
            return false;
        }
        case id<set_values>: {
            if (a_id == b_id) return set_values{views::concat(std::get<set_values>(a).values, std::get<set_values>(b).values) | to<std::vector>};
            return false;
        }
        case id<toggle_value>: return a_id == b_id && std::get<toggle_value>(a).path == std::get<toggle_value>(b).path;
        case id<apply_patch>: {
            if (a_id == b_id) {
                const auto &_a = std::get<apply_patch>(a);
                const auto &_b = std::get<apply_patch>(b);
                // Keep patch actions affecting different base state-paths separate,
                // since actions affecting different state bases are likely semantically different.
                const auto &ops = Merge(_a.patch.Ops, _b.patch.Ops);
                if (ops.empty()) return true;
                if (_a.patch.BasePath == _b.patch.BasePath) return apply_patch{ops, _b.patch.BasePath};
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
        std::visit(visitor{
            [&](const bool cancel_out) {
                if (cancel_out) i++; // The two actions (`a` and `b`) cancel out, so we add neither. (Skip over `b` entirely.)
                else merged_gesture.emplace_back(a); // The left-side action (`a`) can't be merged into any further - nothing more we can do for it!
                active.reset(); // No merge in either case. Move on to try compressing the next action.
            },
            [&](const StateAction &merged_action) {
                active.emplace(merged_action, b.second); // The two actions were merged. Keep track of it but don't add it yet - maybe we can merge more actions into it.
            },
        }, merge_result);
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

void Bool::Toggle() const { q(toggle_value{Path}); }

bool Field::Bool::Draw() const {
    bool value = *this;
    const bool edited = Checkbox(format("{}##{}", Name, Path.string()).c_str(), &value); // todo all fields should be rendered like this, but cache this full value.
    if (edited) Toggle();
    HelpMarker();
    return edited;
}
bool Field::Bool::DrawMenu() const {
    const bool value = *this;
    HelpMarker(false);
    const bool edited = MenuItem(Name.c_str(), nullptr, value);
    if (edited) Toggle();
    return edited;
}

bool Field::UInt::Draw() const {
    U32 value = *this;
    const bool edited = SliderScalar(Name.c_str(), ImGuiDataType_S32, &value, &min, &max, "%d");
    UiContext.WidgetGestured();
    if (edited) q(set_value{Path, value});
    HelpMarker();
    return edited;
}

bool Field::Int::Draw() const {
    int value = *this;
    const bool edited = SliderInt(Name.c_str(), &value, min, max, "%d", ImGuiSliderFlags_None);
    UiContext.WidgetGestured();
    if (edited) q(set_value{Path, value});
    HelpMarker();
    return edited;
}
bool Field::Int::Draw(const vector<int> &options) const {
    bool edited = false;
    const int value = *this;
    if (BeginCombo(Name.c_str(), to_string(value).c_str())) {
        for (const auto option: options) {
            const bool is_selected = option == value;
            if (Selectable(to_string(option).c_str(), is_selected)) {
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
    UiContext.WidgetGestured();
    if (edited) q(set_value{Path, value});
    HelpMarker();
    return edited;
}

bool Field::Float::Draw(float v_speed, ImGuiSliderFlags flags) const {
    float value = *this;
    const bool edited = DragFloat(Name.c_str(), &value, v_speed, min, max, fmt, flags);
    UiContext.WidgetGestured();
    if (edited) q(set_value{Path, value});
    HelpMarker();
    return edited;
}
bool Field::Float::Draw() const { return Draw(ImGuiSliderFlags_None); }

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
        for (Count i = 0; i < names.size(); i++) {
            const bool is_selected = value == int(i);
            if (MenuItem(names[i].c_str(), nullptr, is_selected)) {
                q(set_value{Path, int(i)});
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
        for (Count i = 0; i < items.size(); i++) {
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
        for (Count i = 0; i < items.size(); i++) {
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

bool Vec2::Draw(ImGuiSliderFlags flags) const {
    ImVec2 values = *this;
    const bool edited = SliderFloat2(Name.c_str(), (float *) &values, min, max, fmt, flags);
    UiContext.WidgetGestured();
    if (edited) q(set_values{{{X.Path, values.x}, {Y.Path, values.y}}});
    HelpMarker();
    return edited;
}

void Vec2::Draw() const { Draw(ImGuiSliderFlags_None); }

bool Vec2Linked::Draw(ImGuiSliderFlags flags) const {
    if (Linked.Draw()) {
        // Linking sets the max value to the min value.
        if (X < Y) q(set_value{Y.Path, X});
        else if (Y < X) q(set_value{X.Path, Y});
    }
    SameLine();
    ImVec2 values = *this;
    const bool edited = SliderFloat2(Name.c_str(), (float *) &values, min, max, fmt, flags);
    UiContext.WidgetGestured();
    if (edited) {
        if (Linked) {
            const float changed_value = values.x != X ? values.x : values.y;
            q(set_values{{{X.Path, changed_value}, {Y.Path, changed_value}}});
        } else {
            q(set_values{{{X.Path, values.x}, {Y.Path, values.y}}});
        }
    }
    HelpMarker();
    return edited;
}

void Vec2Linked::Draw() const { Draw(ImGuiSliderFlags_None); }

//-----------------------------------------------------------------------------
// [SECTION] Window methods
//-----------------------------------------------------------------------------

Window::Window(const StateMember *parent, const string &path_segment, const string &name_help, const bool visible)
    : UIStateMember(parent, path_segment, name_help) {
    Set(Visible, visible, c.CtorStore);
}

void Window::DrawWindow(ImGuiWindowFlags flags) const {
    if (!Visible) return;

    bool open = Visible;
    if (Begin(Name.c_str(), &open, flags) && open) Draw();
    End();

    if (Visible && !open) q(set_value{Visible.Path, false});
}

void Window::Dock(ID node_id) const {
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

void Info::Draw() const {
    const auto hovered_id = GetHoveredID();
    if (hovered_id && StateMember::WithId.contains(hovered_id)) {
        const auto *member = StateMember::WithId.at(hovered_id);
        const string &help = member->Help;
        PushTextWrapPos(0);
        TextUnformatted((help.empty() ? format("No info available for {}.", member->Name) : help).c_str());
    }
}

void State::UIProcess::Draw() const {}

static int PrevFontIndex = 0;
static float PrevFontScale = 1.0;

void State::Draw() const {
    if (Style.ImGui.FontIndex != PrevFontIndex) {
        GetIO().FontDefault = GetIO().Fonts->Fonts[Style.ImGui.FontIndex];
        PrevFontIndex = Style.ImGui.FontIndex;
    }
    if (PrevFontScale != Style.ImGui.FontScale) {
        GetIO().FontGlobalScale = Style.ImGui.FontScale / Style::ImGuiStyle::FontAtlasScale;
        PrevFontScale = Style.ImGui.FontScale;
    }

    if (BeginMainMenuBar()) {
        if (BeginMenu("File")) {
            MenuItem(open_empty_project{});
            MenuItem(show_open_project_dialog{});
            if (BeginMenu("Open recent project", !c.Preferences.RecentlyOpenedPaths.empty())) {
                for (const auto &recently_opened_path: c.Preferences.RecentlyOpenedPaths) {
                    if (MenuItem(recently_opened_path.filename().c_str())) q(open_project{recently_opened_path});
                }
                EndMenu();
            }
            MenuItem(open_default_project{});

            MenuItem(save_current_project{});
            MenuItem(show_save_project_dialog{});
            MenuItem(save_default_project{});
            EndMenu();
        }
        if (BeginMenu("Edit")) {
            MenuItem(undo{});
            MenuItem(redo{});
            EndMenu();
        }
        if (BeginMenu("Windows")) {
            if (BeginMenu("Debug")) {
                DebugLog.ToggleMenuItem();
                StackTool.ToggleMenuItem();
                StateViewer.ToggleMenuItem();
                StatePathUpdateFrequency.ToggleMenuItem();
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

    ApplicationSettings.DrawWindow();
    Audio.DrawWindow();

    Audio.Faust.Editor.DrawWindow(ImGuiWindowFlags_MenuBar);
    Audio.Faust.Diagram.DrawWindow(ImGuiWindowFlags_MenuBar);
    Audio.Faust.Params.DrawWindow();
    Audio.Faust.Log.DrawWindow();

    DebugLog.DrawWindow();
    StackTool.DrawWindow();
    StateViewer.DrawWindow(ImGuiWindowFlags_MenuBar);
    StatePathUpdateFrequency.DrawWindow();
    StateMemoryEditor.DrawWindow(ImGuiWindowFlags_NoScrollbar);
    ProjectPreview.DrawWindow();

    Metrics.DrawWindow();
    Style.DrawWindow();
    Demo.DrawWindow(ImGuiWindowFlags_MenuBar);
    FileDialog.Draw();
    Info.DrawWindow();
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

void DockNodeSettings::Set(const ImVector<ImGuiDockNodeSettings> &dss, TransientStore &_store) const {
    const Count size = dss.Size;
    for (Count i = 0; i < size; i++) {
        const auto &ds = dss[int(i)];
        NodeId.Set(i, ds.NodeId, _store);
        ParentNodeId.Set(i, ds.ParentNodeId, _store);
        ParentWindowId.Set(i, ds.ParentWindowId, _store);
        SelectedTabId.Set(i, ds.SelectedTabId, _store);
        SplitAxis.Set(i, ds.SplitAxis, _store);
        Depth.Set(i, ds.Depth, _store);
        Flags.Set(i, int(ds.Flags), _store);
        Pos.Set(i, PackImVec2ih(ds.Pos), _store);
        Size.Set(i, PackImVec2ih(ds.Size), _store);
        SizeRef.Set(i, PackImVec2ih(ds.SizeRef), _store);
    }
    NodeId.truncate(size, _store);
    ParentNodeId.truncate(size, _store);
    ParentWindowId.truncate(size, _store);
    SelectedTabId.truncate(size, _store);
    SplitAxis.truncate(size, _store);
    Depth.truncate(size, _store);
    Flags.truncate(size, _store);
    Pos.truncate(size, _store);
    Size.truncate(size, _store);
    SizeRef.truncate(size, _store);
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

void WindowSettings::Set(ImChunkStream<ImGuiWindowSettings> &wss, TransientStore &_store) const {
    Count i = 0;
    for (auto *ws = wss.begin(); ws != nullptr; ws = wss.next_chunk(ws)) {
        ID.Set(i, ws->ID, _store);
        ClassId.Set(i, ws->DockId, _store);
        ViewportId.Set(i, ws->ViewportId, _store);
        DockId.Set(i, ws->DockId, _store);
        DockOrder.Set(i, ws->DockOrder, _store);
        Pos.Set(i, PackImVec2ih(ws->Pos), _store);
        Size.Set(i, PackImVec2ih(ws->Size), _store);
        ViewportPos.Set(i, PackImVec2ih(ws->ViewportPos), _store);
        Collapsed.Set(i, ws->Collapsed, _store);
        i++;
    }
    ID.truncate(i, _store);
    ClassId.truncate(i, _store);
    ViewportId.truncate(i, _store);
    DockId.truncate(i, _store);
    DockOrder.truncate(i, _store);
    Pos.truncate(i, _store);
    Size.truncate(i, _store);
    ViewportPos.truncate(i, _store);
    Collapsed.truncate(i, _store);
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

void TableSettings::Set(ImChunkStream<ImGuiTableSettings> &tss, TransientStore &_store) const {
    Count i = 0;
    for (auto *ts = tss.begin(); ts != nullptr; ts = tss.next_chunk(ts)) {
        auto columns_count = ts->ColumnsCount;

        ID.Set(i, ts->ID, _store);
        SaveFlags.Set(i, ts->SaveFlags, _store);
        RefScale.Set(i, ts->RefScale, _store);
        ColumnsCount.Set(i, columns_count, _store);
        ColumnsCountMax.Set(i, ts->ColumnsCountMax, _store);
        WantApply.Set(i, ts->WantApply, _store);
        for (int column_index = 0; column_index < columns_count; column_index++) {
            const auto &cs = ts->GetColumnSettings()[column_index];
            Columns.WidthOrWeight.Set(i, column_index, cs.WidthOrWeight, _store);
            Columns.UserID.Set(i, column_index, cs.UserID, _store);
            Columns.Index.Set(i, column_index, cs.Index, _store);
            Columns.DisplayOrder.Set(i, column_index, cs.DisplayOrder, _store);
            Columns.SortOrder.Set(i, column_index, cs.SortOrder, _store);
            Columns.SortDirection.Set(i, column_index, cs.SortDirection, _store);
            Columns.IsEnabled.Set(i, column_index, cs.IsEnabled, _store);
            Columns.IsStretch.Set(i, column_index, cs.IsStretch, _store);
        }
        Columns.WidthOrWeight.truncate(i, columns_count, _store);
        Columns.UserID.truncate(i, columns_count, _store);
        Columns.Index.truncate(i, columns_count, _store);
        Columns.DisplayOrder.truncate(i, columns_count, _store);
        Columns.SortOrder.truncate(i, columns_count, _store);
        Columns.SortDirection.truncate(i, columns_count, _store);
        Columns.IsEnabled.truncate(i, columns_count, _store);
        Columns.IsStretch.truncate(i, columns_count, _store);
        i++;
    }
    ID.truncate(i, _store);
    SaveFlags.truncate(i, _store);
    RefScale.truncate(i, _store);
    ColumnsCount.truncate(i, _store);
    ColumnsCountMax.truncate(i, _store);
    WantApply.truncate(i, _store);
    Columns.WidthOrWeight.truncate(i, _store);
    Columns.UserID.truncate(i, _store);
    Columns.Index.truncate(i, _store);
    Columns.DisplayOrder.truncate(i, _store);
    Columns.SortOrder.truncate(i, _store);
    Columns.SortDirection.truncate(i, _store);
    Columns.IsEnabled.truncate(i, _store);
    Columns.IsStretch.truncate(i, _store);
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
            column->DisplayOrder = ImGuiTableFlags(SaveFlags[i]) & ImGuiTableFlags_Reorderable ? ImGuiTableColumnIdx(Columns.DisplayOrder.at(i, j)) : (ImGuiTableColumnIdx) column_n;
            display_order_mask |= (ImU64) 1 << column->DisplayOrder;
            column->IsUserEnabled = column->IsUserEnabledNextFrame = Columns.IsEnabled.at(i, j);
            column->SortOrder = ImGuiTableColumnIdx(Columns.SortOrder.at(i, j));
            column->SortDirection = Columns.SortDirection.at(i, j);
        }

        // Validate and fix invalid display order data
        const ImU64 expected_display_order_mask = ColumnsCount[i] == 64 ? ~0 : ((ImU64) 1 << ImU8(ColumnsCount[i])) - 1;
        if (display_order_mask != expected_display_order_mask) {
            for (int column_n = 0; column_n < table->ColumnsCount; column_n++) {
                table->Columns[column_n].DisplayOrder = (ImGuiTableColumnIdx) column_n;
            }
        }
        // Rebuild index
        for (int column_n = 0; column_n < table->ColumnsCount; column_n++) {
            table->DisplayOrderToIndex[table->Columns[column_n].DisplayOrder] = (ImGuiTableColumnIdx) column_n;
        }
    }
}

Store ImGuiSettings::Set(ImGuiContext *ctx) const {
    ImGui::SaveIniSettingsToMemory(); // Populates the `Settings` context members
    auto _store = store.transient();
    Nodes.Set(ctx->DockContext.NodesSettings, _store);
    Windows.Set(ctx->SettingsWindows, _store);
    Tables.Set(ctx->SettingsTables, _store);

    return _store.persistent();
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
void StateViewer::StateJsonTree(const string &key, const json &value, const StatePath &path) const {
    const string &leaf_name = path == RootPath ? path.string() : path.filename().string();
    const auto &parent_path = path == RootPath ? path : path.parent_path();
    const bool is_array_item = IsInteger(leaf_name);
    const int array_index = is_array_item ? std::stoi(leaf_name) : -1;
    const bool is_imgui_color = parent_path == s.Style.ImGui.Colors.Path;
    const bool is_implot_color = parent_path == s.Style.ImPlot.Colors.Path;
    const bool is_flowgrid_color = parent_path == s.Style.FlowGrid.Colors.Path;
    const auto &label = LabelMode == Annotated ?
                        (is_imgui_color ? s.Style.ImGui.Colors.GetName(array_index) :
                         is_implot_color ? s.Style.ImPlot.Colors.GetName(array_index) :
                         is_flowgrid_color ? s.Style.FlowGrid.Colors.GetName(array_index) :
                         is_array_item ? leaf_name : key) : key;

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
            for (const auto &it: value) {
                StateJsonTree(to_string(i), it, path / to_string(i));
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

    StateJsonTree("State", c.GetProjectJson());
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

void ProjectPreview::Draw() const {
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

Style::ImGuiStyle::ImGuiStyle(const StateMember *parent, const string &path_segment, const string &name_help)
    : UIStateMember(parent, path_segment, name_help) {
    ColorsDark(c.CtorStore);
}
Style::ImPlotStyle::ImPlotStyle(const StateMember *parent, const string &path_segment, const string &name_help)
    : UIStateMember(parent, path_segment, name_help) {
    ColorsAuto(c.CtorStore);
}
Style::FlowGridStyle::FlowGridStyle(const StateMember *parent, const string &path_segment, const string &name_help)
    : UIStateMember(parent, path_segment, name_help) {
    ColorsDark(c.CtorStore);
}
Style::FlowGridStyle::Diagram::Diagram(const StateMember *parent, const string &path_segment, const string &name_help)
    : UIStateMember(parent, path_segment, name_help) {
    ColorsDark(c.CtorStore);
    LayoutFlowGrid(c.CtorStore);
}

void Style::ImGuiStyle::ColorsDark(TransientStore &_store) const {
    vector<ImVec4> dst(ImGuiCol_COUNT);
    ImGui::StyleColorsDark(&dst[0]);
    Colors.Set(dst, _store);
}
void Style::ImGuiStyle::ColorsLight(TransientStore &_store) const {
    vector<ImVec4> dst(ImGuiCol_COUNT);
    ImGui::StyleColorsLight(&dst[0]);
    Colors.Set(dst, _store);
}
void Style::ImGuiStyle::ColorsClassic(TransientStore &_store) const {
    vector<ImVec4> dst(ImGuiCol_COUNT);
    ImGui::StyleColorsClassic(&dst[0]);
    Colors.Set(dst, _store);
}

void Style::ImPlotStyle::ColorsAuto(TransientStore &_store) const {
    vector<ImVec4> dst(ImPlotCol_COUNT);
    ImPlot::StyleColorsAuto(&dst[0]);
    Colors.Set(dst, _store);
    Set(MinorAlpha, 0.25f, _store);
}
void Style::ImPlotStyle::ColorsDark(TransientStore &_store) const {
    vector<ImVec4> dst(ImPlotCol_COUNT);
    ImPlot::StyleColorsDark(&dst[0]);
    Colors.Set(dst, _store);
    Set(MinorAlpha, 0.25f, _store);
}
void Style::ImPlotStyle::ColorsLight(TransientStore &_store) const {
    vector<ImVec4> dst(ImPlotCol_COUNT);
    ImPlot::StyleColorsLight(&dst[0]);
    Colors.Set(dst, _store);
    Set(MinorAlpha, 1, _store);
}
void Style::ImPlotStyle::ColorsClassic(TransientStore &_store) const {
    vector<ImVec4> dst(ImPlotCol_COUNT);
    ImPlot::StyleColorsClassic(&dst[0]);
    Colors.Set(dst, _store);
    Set(MinorAlpha, 0.5f, _store);
}

void Style::FlowGridStyle::ColorsDark(TransientStore &_store) const {
    Colors.Set({
        {FlowGridCol_HighlightText, {1, 0.6, 0, 1}},
        {FlowGridCol_GestureIndicator, {0.87, 0.52, 0.32, 1}},
        {FlowGridCol_ParamsBg, {0.16, 0.29, 0.48, 0.1}},
    }, _store);
}
void Style::FlowGridStyle::ColorsLight(TransientStore &_store) const {
    Colors.Set({
        {FlowGridCol_HighlightText, {1, 0.45, 0, 1}},
        {FlowGridCol_GestureIndicator, {0.87, 0.52, 0.32, 1}},
        {FlowGridCol_ParamsBg, {1, 1, 1, 1}},
    }, _store);
}
void Style::FlowGridStyle::ColorsClassic(TransientStore &_store) const {
    Colors.Set({
        {FlowGridCol_HighlightText, {1, 0.6, 0, 1}},
        {FlowGridCol_GestureIndicator, {0.87, 0.52, 0.32, 1}},
        {FlowGridCol_ParamsBg, {0.43, 0.43, 0.43, 0.1}},
    }, _store);
}

void Style::FlowGridStyle::Diagram::ColorsDark(TransientStore &_store) const {
    Colors.Set({
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
    }, _store);
}
void Style::FlowGridStyle::Diagram::ColorsClassic(TransientStore &_store) const {
    Colors.Set({
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
    }, _store);
}
void Style::FlowGridStyle::Diagram::ColorsLight(TransientStore &_store) const {
    Colors.Set({
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
    }, _store);
}
void Style::FlowGridStyle::Diagram::ColorsFaust(TransientStore &_store) const {
    Colors.Set({
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
    }, _store);
}

void Style::FlowGridStyle::Diagram::LayoutFlowGrid(TransientStore &_store) const {
    Set(DefaultLayoutEntries, _store);
}
void Style::FlowGridStyle::Diagram::LayoutFaust(TransientStore &_store) const {
    Set({
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
    }, _store);
}

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
                if (Button("Auto")) q(set_value{Path / to_string(i), is_auto ? mapped_value : AutoColor});
                if (!is_auto) PopStyleVar();
                SameLine();
            }
            auto mutable_value = ColorConvertU32ToFloat4(mapped_value);
            if (is_auto) BeginDisabled();
            const bool item_changed = ColorEdit4(PathLabel(Path / to_string(i)).c_str(), (float *) &mutable_value,
                alpha_flags | ImGuiColorEditFlags_AlphaBar | (AllowAuto ? ImGuiColorEditFlags_AlphaPreviewHalf : 0));
            UiContext.WidgetGestured();
            if (is_auto) EndDisabled();

            SameLine(0, style.ItemInnerSpacing.x);
            TextUnformatted(name.c_str());
            PopID();

            if (item_changed) q(set_value{Path / to_string(i), ColorConvertFloat4ToU32(mutable_value)});
            changed |= item_changed;
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

void Style::FlowGridStyle::Diagram::Draw() const {
    FoldComplexity.Draw();
    const bool scale_fill = ScaleFillHeight;
    ScaleFillHeight.Draw();
    if (scale_fill) ImGui::BeginDisabled();
    Scale.Draw();
    if (scale_fill) {
        SameLine();
        TextUnformatted(format("Uncheck '{}' to manually edit diagram scale.", ScaleFillHeight.Name).c_str());
        ImGui::EndDisabled();
    }
    Direction.Draw();
    OrientationMark.Draw();
    if (OrientationMark) {
        SameLine();
        SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
        OrientationMarkRadius.Draw();
    }
    RouteFrame.Draw();
    SequentialConnectionZigzag.Draw();
    Separator();
    const bool decorate_folded = DecorateRootNode;
    DecorateRootNode.Draw();
    if (!decorate_folded) ImGui::BeginDisabled();
    DecorateMargin.Draw();
    DecoratePadding.Draw();
    DecorateLineWidth.Draw();
    DecorateCornerRadius.Draw();
    if (!decorate_folded) ImGui::EndDisabled();
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
void Style::FlowGridStyle::Params::Draw() const {
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
void Style::FlowGridStyle::Draw() const {
    static int colors_idx = -1, diagram_colors_idx = -1, diagram_layout_idx = -1;
    if (Combo("Colors", &colors_idx, "Dark\0Light\0Classic\0")) q(set_flowgrid_color_style{colors_idx});
    if (Combo("Diagram colors", &diagram_colors_idx, "Dark\0Light\0Classic\0Faust\0")) q(set_flowgrid_diagram_color_style{diagram_colors_idx});
    if (Combo("Diagram layout", &diagram_layout_idx, "FlowGrid\0Faust\0")) q(set_flowgrid_diagram_layout_style{diagram_layout_idx});
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
    int value = int(c.History.Index);
    if (SliderInt("History index", &value, 0, int(c.History.Size() - 1))) q(set_history_index{value});
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
void FileDialog::Set(const FileDialogData &data, TransientStore &_store) const {
    ::Set({
        {Title, data.title},
        {Filters, data.filters},
        {FilePath, data.file_path},
        {DefaultFileName, data.default_file_name},
        {SaveMode, data.save_mode},
        {MaxNumSelections, data.max_num_selections},
        {Flags, data.flags},
        {Visible, true},
    }, _store);
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

void ShowGesture(const Gesture &gesture) {
    for (Count action_index = 0; action_index < gesture.size(); action_index++) {
        const auto &[action, time] = gesture[action_index];
        JsonTree(format("{}: {}", action::GetName(action), time), json(action)[1], JsonTreeNodeFlags_None, to_string(action_index).c_str());
    }
}

void Metrics::FlowGridMetrics::Draw() const {
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
                        for (const auto &[partial_path, op]: patch.Ops) {
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
                for (const auto &recently_opened_path: c.Preferences.RecentlyOpenedPaths) {
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
