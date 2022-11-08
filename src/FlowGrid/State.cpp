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
// [SECTION] Fields
//-----------------------------------------------------------------------------

namespace Field {
Bool::operator bool() const { return std::get<bool>(store.at(Path)); }
Int::operator int() const { return std::get<int>(store.at(Path)); }
UInt::operator unsigned int() const { return std::get<unsigned int>(store.at(Path)); }
Float::operator float() const {
    const auto &value = store.at(Path);
    if (std::holds_alternative<int>(value)) return float(std::get<int>(value));
    return std::get<float>(value);
}
Vec2::operator ImVec2() const { return std::get<ImVec2>(store.at(Path)); }
Vec2Int::operator ImVec2ih() const { return std::get<ImVec2ih>(store.at(Path)); }

String::operator string() const { return std::get<string>(store.at(Path)); }
bool String::operator==(const string &v) const { return string(*this) == v; }
String::operator bool() const { return !string(*this).empty(); }

Enum::operator int() const { return std::get<int>(store.at(Path)); }
Flags::operator int() const { return std::get<int>(store.at(Path)); }

template<typename T>
T Vector<T>::operator[](size_t index) const { return std::get<T>(store.at(Path / to_string(index))); };
template<typename T>
size_t Vector<T>::size(const Store &_store) const {
    int size = -1;
    while (_store.count(Path / to_string(++size))) {}
    return size_t(size);
}

// Transient
template<typename T>
void Vector<T>::set(size_t index, const T &value, TransientStore &_store) const { _store.set(Path / to_string(index), value); }
template<typename T>
void Vector<T>::set(const vector<T> &values, TransientStore &_store) const {
    ::set(views::ints(0, int(values.size())) | transform([&](const int i) { return StoreEntry(Path / to_string(i), values[i]); }) | to<vector>, _store);
    truncate(values.size(), _store);
}
template<typename T>
Store Vector<T>::set(const vector<std::pair<int, T>> &values, const Store &_store) const {
    auto transient = _store.transient();
    for (const auto &[index, value]: values) transient.set(Path / to_string(index), value);
    return transient.persistent();
}

// Persistent
template<typename T>
Store Vector<T>::set(size_t index, const T &value, const Store &_store) const { return ::set(Path / index, value, _store); }
template<typename T>
Store Vector<T>::set(const vector<T> &values, const Store &_store) const {
    if (values.empty()) return _store;

    auto transient = _store.transient();
    set(values, transient);
    return transient.persistent();
}

template<typename T>
void Vector<T>::truncate(size_t length, TransientStore &_store) const {
    size_t i = length - 1;
    while (_store.count(Path / to_string(++i))) _store.erase(Path / to_string(i));
}

template<typename T>
T Vector2D<T>::at(size_t i, size_t j, const Store &_store) const { return std::get<T>(_store.at(Path / to_string(i) / to_string(j))); };
template<typename T>
size_t Vector2D<T>::size(const TransientStore &_store) const {
    int size = -1;
    while (_store.count(Path / ++size / 0).to_string()) {}
    return size;
}
template<typename T>
Store Vector2D<T>::set(size_t i, size_t j, const T &value, const Store &_store) const { return _store.set(Path / to_string(i) / to_string(j), value); }
template<typename T>
void Vector2D<T>::set(size_t i, size_t j, const T &value, TransientStore &_store) const { _store.set(Path / to_string(i) / to_string(j), value); }
template<typename T>
void Vector2D<T>::truncate(size_t length, TransientStore &_store) const {
    size_t i = length - 1;
    while (_store.count(Path / to_string(++i) / "0")) truncate(i, 0, _store);
}
template<typename T>
void Vector2D<T>::truncate(size_t i, size_t length, TransientStore &_store) const {
    size_t j = length - 1;
    while (_store.count(Path / to_string(i) / to_string(++j))) _store.erase(Path / to_string(i) / to_string(j));
}
}

//-----------------------------------------------------------------------------
// [SECTION] Actions
//-----------------------------------------------------------------------------

PatchOps merge(const PatchOps &a, const PatchOps &b) {
    PatchOps merged = a;
    for (const auto &[path, op]: b) {
        if (merged.contains(path)) {
            const auto &old_op = merged.at(path);
            // Strictly, two consecutive patches that both add or both remove the same key should throw an exception,
            // but I'm being lax here to allow for merging multiple patches by only looking at neighbors.
            // For example, if the first patch removes a path, and the second one adds the same path,
            // we can't know from only looking at the pair whether the added value was the same as it was before the remove
            // (in which case it should just be `Remove` during merge) or if it was different (in which case the merged action should be a `Replace`).
            if (old_op.op == Add) {
                if (op.op == Remove || ((op.op == Add || op.op == Replace) && old_op.value == op.value)) merged.erase(path); // Cancel out
                else merged[path] = {Add, op.value, {}};
            } else if (old_op.op == Remove) {
                if (op.op == Add || op.op == Replace) {
                    if (old_op.value == op.value) merged.erase(path); // Cancel out
                    else merged[path] = {Replace, op.value, old_op.old};
                } else {
                    merged[path] = {Remove, {}, old_op.old};
                }
            } else if (old_op.op == Replace) {
                if (op.op == Add || op.op == Replace) merged[path] = {Replace, op.value, old_op.old};
                else merged[path] = {Remove, {}, old_op.old};
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
std::variant<Action, bool> merge(const Action &a, const Action &b) {
    const ID a_id = get_id(a);
    const ID b_id = get_id(b);

    switch (a_id) {
        case id<undo>: if (b_id == id<set_history_index>) return b;
            return b_id == id<redo>;
        case id<redo>: if (b_id == id<set_history_index>) return b;
            return b_id == id<undo>;
        case id<set_history_index>:
        case id<open_empty_project>:
        case id<open_default_project>:
        case id<show_open_project_dialog>:
        case id<open_file_dialog>:
        case id<close_file_dialog>:
        case id<show_save_project_dialog>:
        case id<close_application>:
        case id<set_imgui_color_style>:
        case id<set_implot_color_style>:
        case id<set_flowgrid_color_style>:
        case id<set_flowgrid_diagram_color_style>:
        case id<set_flowgrid_diagram_layout_style>:
        case id<show_open_faust_file_dialog>:
        case id<show_save_faust_file_dialog>: if (a_id == b_id) return b;
            return false;
        case id<open_project>:
        case id<open_faust_file>:
        case id<save_faust_file>: if (a_id == b_id && json(a) == json(b)) return a;
            return false;
        case id<set_value>: if (a_id == b_id && std::get<set_value>(a).path == std::get<set_value>(b).path) return b;
            return false;
        case id<set_values>: if (a_id == b_id) return set_values{views::concat(std::get<set_values>(a).values, std::get<set_values>(b).values) | to<std::vector>};
            return false;
        case id<toggle_value>: return a_id == b_id && std::get<toggle_value>(a).path == std::get<toggle_value>(b).path;
        case id<apply_patch>:
            if (a_id == b_id) {
                const auto &_a = std::get<apply_patch>(a);
                const auto &_b = std::get<apply_patch>(b);
                // Keep patch actions affecting different base state-paths separate,
                // since actions affecting different state bases are likely semantically different.
                if (_a.patch.base_path == _b.patch.base_path) return apply_patch{merge(_a.patch.ops, _b.patch.ops), _b.patch.base_path};
                return false;
            }
            return false;
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

bool Field::UInt::Draw() const {
    unsigned int value = *this;
    const bool edited = SliderScalar(Name.c_str(), ImGuiDataType_S32, &value, &min, &max, "%d");
    gestured();
    if (edited) q(set_value{Path, value});
    HelpMarker();
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

bool Field::Vec2Int::Draw() const {
    ImVec2ih value = *this;
    const bool edited = SliderInt2(Name.c_str(), (int *) &value, min, max, nullptr, ImGuiSliderFlags_None);
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
                JsonTree(to_string(i), it, node_flags);
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

Window::Window(const StateMember *parent, const string &id, const bool visible) : UIStateMember(parent, id) {
    set(set(Visible, visible));
}

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
// Copy of ImGui version, which is not defined publicly
struct ImGuiDockNodeSettings { // NOLINT(cppcoreguidelines-pro-type-member-init)
    ImGuiID ID;
    ImGuiID ParentNodeId;
    ImGuiID ParentWindowId;
    ImGuiID SelectedTabId;
    signed char SplitAxis;
    char Depth;
    ImGuiDockNodeFlags Flags;
    ImVec2ih Pos;
    ImVec2ih Size;
    ImVec2ih SizeRef;
};

void DockNodeSettings::set(const ImVector<ImGuiDockNodeSettings> &dss, TransientStore &_store) const {
    const int size = dss.Size;
    for (int i = 0; i < size; i++) {
        const auto &ds = dss[i];
        ID.set(i, ds.ID, _store);
        ParentNodeId.set(i, ds.ParentNodeId, _store);
        ParentWindowId.set(i, ds.ParentWindowId, _store);
        SelectedTabId.set(i, ds.SelectedTabId, _store);
        SplitAxis.set(i, ds.SplitAxis, _store);
        Depth.set(i, ds.Depth, _store);
        Flags.set(i, int(ds.Flags), _store);
        Pos.set(i, ds.Pos, _store);
        Size.set(i, ds.Size, _store);
        SizeRef.set(i, ds.SizeRef, _store);
    }
    ID.truncate(size, _store);
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
    for (int i = 0; i < int(ID.size()); i++) {
        ctx->DockContext.NodesSettings.push_back({
            ImGuiID(ID[i]),
            ImGuiID(ParentNodeId[i]),
            ImGuiID(ParentWindowId[i]),
            ImGuiID(SelectedTabId[i]),
            (signed char) SplitAxis[i],
            char(Depth[i]),
            Flags[i],
            Pos[i],
            Size[i],
            SizeRef[i],
        });
    }
}

void WindowSettings::set(ImChunkStream<ImGuiWindowSettings> &wss, TransientStore &_store) const {
    int i = 0;
    for (auto *ws = wss.begin(); ws != nullptr; ws = wss.next_chunk(ws)) {
        ID.set(i, ws->ID, _store);
        ClassId.set(i, ws->DockId, _store);
        ViewportId.set(i, ws->ViewportId, _store);
        DockId.set(i, ws->DockId, _store);
        DockOrder.set(i, ws->DockOrder, _store);
        Pos.set(i, ws->Pos, _store);
        Size.set(i, ws->Size, _store);
        ViewportPos.set(i, ws->ViewportPos, _store);
        Collapsed.set(i, ws->Collapsed, _store);
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
    for (int i = 0; i < int(ID.size()); i++) {
        ImGuiID id = ID[i];
        auto *window = FindWindowByID(id);
        if (!window) {
            cout << "Unable to apply settings for window with ID " << format("{:#08X}", id) << ": Window not found.\n";
            continue;
        }

        window->ViewportPos = main_viewport->Pos;
        if (ViewportId[i]) {
            window->ViewportId = ViewportId[i];
            window->ViewportPos = ImVec2(ViewportPos[i].x, ViewportPos[i].y);
        }
        window->Pos = ImVec2(Pos[i].x, Pos[i].y) + ImFloor(window->ViewportPos);
        const auto size = ImVec2(Size[i].x, Size[i].y);
        if (size.x > 0 && size.y > 0) window->Size = window->SizeFull = size;
        window->Collapsed = Collapsed[i];
        window->DockId = DockId[i];
        window->DockOrder = short(DockOrder[i]);
    }
}

void TableSettings::set(ImChunkStream<ImGuiTableSettings> &tss, TransientStore &_store) const {
    int i = 0;
    for (auto *ts = tss.begin(); ts != nullptr; ts = tss.next_chunk(ts)) {
        auto columns_count = ts->ColumnsCount;

        ID.set(i, ts->ID, _store);
        SaveFlags.set(i, ts->SaveFlags, _store);
        RefScale.set(i, ts->RefScale, _store);
        ColumnsCount.set(i, columns_count, _store);
        ColumnsCountMax.set(i, ts->ColumnsCountMax, _store);
        WantApply.set(i, ts->WantApply, _store);
        for (int column_index = 0; column_index < columns_count; column_index++) {
            const auto &cs = ts->GetColumnSettings()[column_index];
            Columns.WidthOrWeight.set(i, column_index, cs.WidthOrWeight, _store);
            Columns.UserID.set(i, column_index, cs.UserID, _store);
            Columns.Index.set(i, column_index, cs.Index, _store);
            Columns.DisplayOrder.set(i, column_index, cs.DisplayOrder, _store);
            Columns.SortOrder.set(i, column_index, cs.SortOrder, _store);
            Columns.SortDirection.set(i, column_index, cs.SortDirection, _store);
            Columns.IsEnabled.set(i, column_index, cs.IsEnabled, _store);
            Columns.IsStretch.set(i, column_index, cs.IsStretch, _store);
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
    for (int i = 0; i < int(ID.size()); i++) {
        ImGuiID id = ID[i];
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
        for (int j = 0; j < ColumnsCount[i]; j++) {
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

Store ImGuiSettings::set(ImGuiContext *ctx) const {
    ImGui::SaveIniSettingsToMemory(); // Populates the `Settings` context members
    auto _store = store.transient();
    Nodes.set(ctx->DockContext.NodesSettings, _store);
    Windows.set(ctx->SettingsWindows, _store);
    Tables.set(ctx->SettingsTables, _store);

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
    style.Colormap = ImPlotColormap_Deep; // todo configurable
    style.UseLocalTime = UseLocalTime;
    style.UseISO8601 = UseISO8601;
    style.Use24HourClock = Use24HourClock;
    for (int i = 0; i < ImPlotCol_COUNT; i++) style.Colors[i] = Colors[i];
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
    const bool is_array_item = is_integer(leaf_name);
    const int array_index = is_array_item ? std::stoi(leaf_name) : -1;
    const bool is_imgui_color = parent_path == s.Style.ImGui.Colors.Path;
    const bool is_implot_color = parent_path == s.Style.ImPlot.Colors.Path;
    const bool is_flowgrid_color = parent_path == s.Style.FlowGrid.Colors.Path;
    const auto &label = LabelMode == Annotated ?
                        (is_imgui_color ? s.Style.ImGui.Colors.GetName(array_index) :
                         is_implot_color ? s.Style.ImPlot.Colors.GetName(array_index) :
                         is_flowgrid_color ? s.Style.FlowGrid.Colors.GetName(array_index) :
                         is_array_item ? leaf_name : key) : key;
    const auto &stats = history.stats;

    if (AutoSelect) {
        const auto &update_paths = stats.latest_updated_paths;
        const auto is_ancestor_path = [&path](const string &candidate_path) { return candidate_path.rfind(path.string(), 0) == 0; };
        const bool was_recently_updated = std::find_if(update_paths.begin(), update_paths.end(), is_ancestor_path) != update_paths.end();
        SetNextItemOpen(was_recently_updated);
        if (was_recently_updated) FillRowItemBg(s.Style.ImGui.Colors[ImGuiCol_FrameBg]);
    }

    // Flash background color of nodes when its corresponding path updates.
    if (stats.latest_update_time_for_path.contains(path)) {
        const auto latest_update_time = stats.latest_update_time_for_path.contains(path) ? stats.latest_update_time_for_path.at(path) : TimePoint{};
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

    StateJsonTree("State", Context::get_project_json());
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
    const auto &stats = history.stats;
    if (stats.committed_update_times_for_path.empty() && stats.gesture_update_times_for_path.empty()) {
        Text("No state updates yet.");
        return;
    }

    auto [labels, values] = stats.CreatePlottable();
    if (ImPlot::BeginPlot("Path update frequency", {-1, float(labels.size()) * 30 + 60}, ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Number of updates", nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert);

        // Hack to allow `SetupAxisTicks` without breaking on assert `n_ticks > 1`: Just add an empty label and only plot one value.
        // todo fix in ImPlot
        if (labels.size() == 1) labels.emplace_back("");

        // todo add an axis flag to exclude non-integer ticks
        // todo add an axis flag to show last tick
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, double(labels.size() - 1), int(labels.size()), labels.data(), false);
        static const char *item_labels[] = {"Committed updates", "Active updates"};
        const bool has_gesture = !stats.gesture_update_times_for_path.empty();
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

    const json project_json = Context::get_project_json(ProjectFormat(int(Format)));
    if (Raw) TextUnformatted(project_json.dump(4).c_str());
    else JsonTree("", project_json, JsonTreeNodeFlags_DefaultOpen);
}

//-----------------------------------------------------------------------------
// [SECTION] Style editors
//-----------------------------------------------------------------------------

Style::ImGuiStyle::ImGuiStyle(const StateMember *parent, const string &id) : UIStateMember(parent, id) {
    set(ColorsDark());
}
Style::ImPlotStyle::ImPlotStyle(const StateMember *parent, const string &id) : UIStateMember(parent, id) {
    set(ColorsAuto());
}
Style::FlowGridStyle::FlowGridStyle(const StateMember *parent, const string &id) : UIStateMember(parent, id) {
    set(ColorsDark());
    set(DiagramColorsDark());
    set(DiagramLayoutFlowGrid());
}

Store Style::ImGuiStyle::ColorsDark() const {
    vector<ImVec4> dst(ImGuiCol_COUNT);
    ImGui::StyleColorsDark(&dst[0]);
    return Colors.set(dst);
}
Store Style::ImGuiStyle::ColorsLight() const {
    vector<ImVec4> dst(ImGuiCol_COUNT);
    ImGui::StyleColorsLight(&dst[0]);
    return Colors.set(dst);
}
Store Style::ImGuiStyle::ColorsClassic() const {
    vector<ImVec4> dst(ImGuiCol_COUNT);
    ImGui::StyleColorsClassic(&dst[0]);
    return Colors.set(dst);
}

Store Style::ImPlotStyle::ColorsAuto() const {
    vector<ImVec4> dst(ImPlotCol_COUNT);
    ImPlot::StyleColorsAuto(&dst[0]);
    return set(MinorAlpha, 0.25f, Colors.set(dst));
}
Store Style::ImPlotStyle::ColorsDark() const {
    vector<ImVec4> dst(ImPlotCol_COUNT);
    ImPlot::StyleColorsDark(&dst[0]);
    return set(MinorAlpha, 0.25f, Colors.set(dst));
}
Store Style::ImPlotStyle::ColorsLight() const {
    vector<ImVec4> dst(ImPlotCol_COUNT);
    ImPlot::StyleColorsLight(&dst[0]);
    return set(MinorAlpha, 1, Colors.set(dst));
}
Store Style::ImPlotStyle::ColorsClassic() const {
    vector<ImVec4> dst(ImPlotCol_COUNT);
    ImPlot::StyleColorsClassic(&dst[0]);
    return set(MinorAlpha, 0.5f, Colors.set(dst));
}

Store Style::FlowGridStyle::ColorsDark() const {
    return Colors.set({
        {FlowGridCol_HighlightText, {1, 0.6, 0, 1}},
        {FlowGridCol_GestureIndicator, {0.87, 0.52, 0.32, 1}},
        {FlowGridCol_ParamsBg, {0.16, 0.29, 0.48, 0.1}},
    });
}
Store Style::FlowGridStyle::ColorsLight() const {
    return Colors.set({
        {FlowGridCol_HighlightText, {1, 0.45, 0, 1}},
        {FlowGridCol_GestureIndicator, {0.87, 0.52, 0.32, 1}},
        {FlowGridCol_ParamsBg, {1, 1, 1, 1}},
    });
}
Store Style::FlowGridStyle::ColorsClassic() const {
    return Colors.set({
        {FlowGridCol_HighlightText, {1, 0.6, 0, 1}},
        {FlowGridCol_GestureIndicator, {0.87, 0.52, 0.32, 1}},
        {FlowGridCol_ParamsBg, {0.43, 0.43, 0.43, 0.1}},
    });
}

Store Style::FlowGridStyle::DiagramColorsDark() const {
    return Colors.set({
        {FlowGridCol_DiagramBg, {0.06, 0.06, 0.06, 0.94}},
        {FlowGridCol_DiagramText, {1, 1, 1, 1}},
        {FlowGridCol_DiagramGroupTitle, {1, 1, 1, 1}},
        {FlowGridCol_DiagramGroupStroke, {0.43, 0.43, 0.5, 0.5}},
        {FlowGridCol_DiagramLine, {0.61, 0.61, 0.61, 1}},
        {FlowGridCol_DiagramLink, {0.26, 0.59, 0.98, 0.4}},
        {FlowGridCol_DiagramInverter, {1, 1, 1, 1}},
        {FlowGridCol_DiagramOrientationMark, {1, 1, 1, 1}},
        // Box fills
        {FlowGridCol_DiagramNormal, {0.29, 0.44, 0.63, 1}},
        {FlowGridCol_DiagramUi, {0.28, 0.47, 0.51, 1}},
        {FlowGridCol_DiagramSlot, {0.28, 0.58, 0.37, 1}},
        {FlowGridCol_DiagramNumber, {0.96, 0.28, 0, 1}},
    });
}
Store Style::FlowGridStyle::DiagramColorsClassic() const {
    return Colors.set({
        {FlowGridCol_DiagramBg, {0, 0, 0, 0.85}},
        {FlowGridCol_DiagramText, {0.9, 0.9, 0.9, 1}},
        {FlowGridCol_DiagramGroupTitle, {0.9, 0.9, 0.9, 1}},
        {FlowGridCol_DiagramGroupStroke, {0.5, 0.5, 0.5, 0.5}},
        {FlowGridCol_DiagramLine, {1, 1, 1, 1}},
        {FlowGridCol_DiagramLink, {0.35, 0.4, 0.61, 0.62}},
        {FlowGridCol_DiagramInverter, {0.9, 0.9, 0.9, 1}},
        {FlowGridCol_DiagramOrientationMark, {0.9, 0.9, 0.9, 1}},
        // Box fills
        {FlowGridCol_DiagramNormal, {0.29, 0.44, 0.63, 1}},
        {FlowGridCol_DiagramUi, {0.28, 0.47, 0.51, 1}},
        {FlowGridCol_DiagramSlot, {0.28, 0.58, 0.37, 1}},
        {FlowGridCol_DiagramNumber, {0.96, 0.28, 0, 1}},
    });
}
Store Style::FlowGridStyle::DiagramColorsLight() const {
    return Colors.set({
        {FlowGridCol_DiagramBg, {0.94, 0.94, 0.94, 1}},
        {FlowGridCol_DiagramText, {0, 0, 0, 1}},
        {FlowGridCol_DiagramGroupTitle, {0, 0, 0, 1}},
        {FlowGridCol_DiagramGroupStroke, {0, 0, 0, 0.3}},
        {FlowGridCol_DiagramLine, {0.39, 0.39, 0.39, 1}},
        {FlowGridCol_DiagramLink, {0.26, 0.59, 0.98, 0.4}},
        {FlowGridCol_DiagramInverter, {0, 0, 0, 1}},
        {FlowGridCol_DiagramOrientationMark, {0, 0, 0, 1}},
        // Box fills
        {FlowGridCol_DiagramNormal, {0.29, 0.44, 0.63, 1}},
        {FlowGridCol_DiagramUi, {0.28, 0.47, 0.51, 1}},
        {FlowGridCol_DiagramSlot, {0.28, 0.58, 0.37, 1}},
        {FlowGridCol_DiagramNumber, {0.96, 0.28, 0, 1}},
    });
}
Store Style::FlowGridStyle::DiagramColorsFaust() const {
    return Colors.set({
        {FlowGridCol_DiagramBg, {1, 1, 1, 1}},
        {FlowGridCol_DiagramText, {1, 1, 1, 1}},
        {FlowGridCol_DiagramGroupTitle, {0, 0, 0, 1}},
        {FlowGridCol_DiagramGroupStroke, {0.2, 0.2, 0.2, 1}},
        {FlowGridCol_DiagramLine, {0, 0, 0, 1}},
        {FlowGridCol_DiagramLink, {0, 0.2, 0.4, 1}},
        {FlowGridCol_DiagramInverter, {0, 0, 0, 1}},
        {FlowGridCol_DiagramOrientationMark, {0, 0, 0, 1}},
        // Box fills
        {FlowGridCol_DiagramNormal, {0.29, 0.44, 0.63, 1}},
        {FlowGridCol_DiagramUi, {0.28, 0.47, 0.51, 1}},
        {FlowGridCol_DiagramSlot, {0.28, 0.58, 0.37, 1}},
        {FlowGridCol_DiagramNumber, {0.96, 0.28, 0, 1}},
    });
}

Store Style::FlowGridStyle::DiagramLayoutFlowGrid() const {
    return set({
        {DiagramSequentialConnectionZigzag, false},
        {DiagramOrientationMark, false},
        {DiagramTopLevelMargin, 10},
        {DiagramDecorateMargin, 15},
        {DiagramDecorateLineWidth, 2},
        {DiagramDecorateCornerRadius, 5},
        {DiagramBoxCornerRadius, 4},
        {DiagramBinaryHorizontalGapRatio, 0.25f},
        {DiagramWireWidth, 1},
        {DiagramWireGap, 16},
        {DiagramGap, ImVec2{8, 8}},
        {DiagramArrowSize, ImVec2{3, 2}},
        {DiagramInverterRadius, 3},
    });
}
Store Style::FlowGridStyle::DiagramLayoutFaust() const {
    return set({
        {DiagramSequentialConnectionZigzag, true},
        {DiagramOrientationMark, true},
        {DiagramTopLevelMargin, 20},
        {DiagramDecorateMargin, 20},
        {DiagramDecorateLineWidth, 1},
        {DiagramBoxCornerRadius, 0},
        {DiagramDecorateCornerRadius, 0},
        {DiagramBinaryHorizontalGapRatio, 0.25f},
        {DiagramWireWidth, 1},
        {DiagramWireGap, 16},
        {DiagramGap, ImVec2{8, 8}},
        {DiagramArrowSize, ImVec2{3, 2}},
        {DiagramInverterRadius, 3},
    });
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
        for (int i = 0; i < int(size()); i++) {
            const string &name = GetName(i);
            if (!filter.PassFilter(name.c_str())) continue;

            PushID(i);
            if (allow_auto) {
                // todo generalize auto colors (linked to ImGui colors) and use in FG colors
                auto temp = ImPlot::GetStyleColorVec4(i);
                const bool is_auto = ImPlot::IsColorAuto(i);
                if (!is_auto) PushStyleVar(ImGuiStyleVar_Alpha, 0.25);
                if (Button("Auto")) q(set_value{Path, is_auto ? temp : IMPLOT_AUTO_COL});
                if (!is_auto) PopStyleVar();
                SameLine();
            }
            auto value = (*this)[i];
            changed |= ImGui::ColorEdit4(path_label(Path / to_string(i)).c_str(), (float *) &value,
                (ImGuiColorEditFlags_AlphaBar | alpha_flags) | (allow_auto ? ImGuiColorEditFlags_AlphaPreviewHalf : 0));
            gestured();

            SameLine(0, style.ItemInnerSpacing.x);
            TextUnformatted(name.c_str());
            PopID();

            if (changed) q(set_value{Path / to_string(i), value});
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
    int value = history.index;
    if (SliderInt("History index", &value, 0, history.Size() - 1)) q(set_history_index{value});
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
Store FileDialog::set(const FileDialogData &data) const {
    return ::set({
        {Title, data.title},
        {Filters, data.filters},
        {FilePath, data.file_path},
        {DefaultFileName, data.default_file_name},
        {SaveMode, data.save_mode},
        {MaxNumSelections, data.max_num_selections},
        {Flags, data.flags},
        {Visible, true},
    });
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
    for (size_t action_index = 0; action_index < gesture.size(); action_index++) {
        const auto &action = gesture[action_index];
        JsonTree(action::get_name(action), json(action)[1], JsonTreeNodeFlags_None, to_string(action_index).c_str());
    }
}

void Metrics::FlowGridMetrics::Draw() const {
    {
        // Gestures (semantically grouped lists of actions)

        // Active (uncompressed) gesture
        const bool widget_gesture = c.is_widget_gesturing;
        const bool active_gesture_present = !history.active_gesture.empty();
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

                if (active_gesture_present) ShowGesture(history.active_gesture);
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
        const bool has_records = history.Size() > 1; // The first record is the initial store, with an app-start (basically) timestamp, and an empty gesture.
        if (!has_records) BeginDisabled();
        if (TreeNodeEx("History", ImGuiTreeNodeFlags_DefaultOpen, "History (Count: %d, Current index: %d)", history.Size() - 1, history.index)) {
            for (int i = 1; i < history.Size(); i++) {
                if (TreeNodeEx(to_string(i).c_str(), i == history.index ? (ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_DefaultOpen) : ImGuiTreeNodeFlags_None)) {
                    const auto &[time, store_record, gesture] = history.store_records[i];
                    BulletText("Time: %s", format("{}\n", time).c_str());
                    if (TreeNode("Patch")) {
                        const auto &[patch, _] = history.CreatePatch(i - 1); // We compute the patches when we need them rather than memoizing them.
                        for (const auto &[partial_path, op]: patch.ops) {
                            const auto &path = patch.base_path / partial_path;
                            if (TreeNodeEx(path.string().c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                                BulletText("Op: %s", to_string(op.op).c_str());
                                if (op.value.has_value()) BulletText("Value: %s", to_string(op.value.value()).c_str());
                                if (op.old.has_value()) BulletText("Old value: %s", to_string(op.old.value()).c_str());
                                TreePop();
                            }
                        }
                        TreePop();
                    }
                    if (TreeNode("Gesture")) {
                        ShowGesture(gesture);
                        TreePop();
                    }
                    if (TreeNode("Store")) {
                        JsonTree("", store_record);
                        TreePop();
                    }
                    TreePop();
                }
            }
            TreePop();
        }
        if (!has_records) EndDisabled();
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
