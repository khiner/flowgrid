#include "App.h"
#include "StateJson.h"

#include "immer/map.hpp"
#include "ImGuiFileDialog.h"

std::map<ImGuiID, StateMember *> StateMember::WithID{};

immer::map<string, Primitive> state_immer;

namespace Field {
Bool::operator bool() const { return std::get<bool>(state_immer.at(Path.to_string())); }
Bool &Bool::operator=(bool value) {
    state_immer = state_immer.set(Path.to_string(), value);
    return *this;
}

Int::operator int() const { return std::get<int>(state_immer.at(Path.to_string())); }
Int &Int::operator=(int value) {
    state_immer = state_immer.set(Path.to_string(), value);
    return *this;
}

Float::operator float() const { return std::get<float>(state_immer.at(Path.to_string())); }
Float &Float::operator=(float value) {
    state_immer = state_immer.set(Path.to_string(), value);
    return *this;
}

Vec2::operator ImVec2() const { return std::get<ImVec2>(state_immer.at(Path.to_string())); }
Vec2 &Vec2::operator=(const ImVec2 &value) {
    state_immer = state_immer.set(Path.to_string(), value);
    return *this;
}

String::operator string() const { return std::get<string>(state_immer.at(Path.to_string())); }
bool String::operator==(const string &v) const { return string(*this) == v; }
String::operator bool() const { return !string(*this).empty(); }
String &String::operator=(string value) {
    state_immer = state_immer.set(Path.to_string(), std::move(value));
    return *this;
}

Enum::operator int() const { return std::get<int>(state_immer.at(Path.to_string())); }
Enum &Enum::operator=(int value) {
    state_immer = state_immer.set(Path.to_string(), value);
    return *this;
}

Flags::operator int() const { return std::get<int>(state_immer.at(Path.to_string())); }
Flags &Flags::operator=(int value) {
    state_immer = state_immer.set(Path.to_string(), value);
    return *this;
}

Color::operator ImVec4() const { return std::get<ImVec4>(state_immer.at(Path.to_string())); }
Color &Color::operator=(const ImVec4 &value) {
    state_immer = state_immer.set(Path.to_string(), value);
    return *this;
}

Colors &Colors::operator=(const vector<ImVec4> &value) {
    for (int i = 0; i < int(value.size()); i++) colors[i] = value[i];
    return *this;
}

Color &Colors::operator[](const size_t index) { return colors[index]; }
const Color &Colors::operator[](const size_t index) const { return colors[index]; }
size_t Colors::size() const { return colors.size(); }
}

string to_string(const IO io, const bool shorten) {
    switch (io) {
        case IO_In: return shorten ? "in" : "input";
        case IO_Out: return shorten ? "out" : "output";
        case IO_None: return "none";
    }
}

namespace action {
// An action's menu label is its name, except for a few exceptions.
const std::map<ID, string> menu_label_for_id{
    {id<show_open_project_dialog>, "Open project"},
    {id<open_empty_project>, "New project"},
    {id<save_current_project>, "Save project"},
    {id<show_save_project_dialog>, "Save project as..."},
    {id<show_open_faust_file_dialog>, "Open DSP file"},
    {id<show_save_faust_file_dialog>, "Save DSP as..."},
    {id<show_save_faust_svg_file_dialog>, "Export SVG"},
};
string get_name(const Action &action) { return name_for_id.at(get_id(action)); }
const char *get_menu_label(ID action_id) {
    if (menu_label_for_id.contains(action_id)) return menu_label_for_id.at(action_id).c_str();
    return name_for_id.at(action_id).c_str();
}
}

ImGuiTableFlags TableFlagsToImgui(const TableFlags flags) {
    ImGuiTableFlags imgui_flags = ImGuiTableFlags_NoHostExtendX | ImGuiTableFlags_SizingStretchProp;
    if (flags & TableFlags_Resizable) imgui_flags |= ImGuiTableFlags_Resizable;
    if (flags & TableFlags_Reorderable) imgui_flags |= ImGuiTableFlags_Reorderable;
    if (flags & TableFlags_Hideable) imgui_flags |= ImGuiTableFlags_Hideable;
    if (flags & TableFlags_Sortable) imgui_flags |= ImGuiTableFlags_Sortable;
    if (flags & TableFlags_ContextMenuInBody) imgui_flags |= ImGuiTableFlags_ContextMenuInBody;
    if (flags & TableFlags_BordersInnerH) imgui_flags |= ImGuiTableFlags_BordersInnerH;
    if (flags & TableFlags_BordersOuterH) imgui_flags |= ImGuiTableFlags_BordersOuterH;
    if (flags & TableFlags_BordersInnerV) imgui_flags |= ImGuiTableFlags_BordersInnerV;
    if (flags & TableFlags_BordersOuterV) imgui_flags |= ImGuiTableFlags_BordersOuterV;
    if (flags & TableFlags_NoBordersInBody) imgui_flags |= ImGuiTableFlags_NoBordersInBody;
    if (flags & TableFlags_PadOuterX) imgui_flags |= ImGuiTableFlags_PadOuterX;
    if (flags & TableFlags_NoPadOuterX) imgui_flags |= ImGuiTableFlags_NoPadOuterX;
    if (flags & TableFlags_NoPadInnerX) imgui_flags |= ImGuiTableFlags_NoPadInnerX;

    return imgui_flags;
}

// Inspired by [`lager`](https://sinusoid.es/lager/architecture.html#reducer), but only the action-visitor pattern remains.
void State::Update(const Action &action) {
    std::visit(visitor{
        [&](const show_open_project_dialog &) { FileDialog = {"Choose file", AllProjectExtensionsDelimited, ".", ""}; },
        [&](const show_save_project_dialog &) { FileDialog = {"Choose file", AllProjectExtensionsDelimited, ".", "my_flowgrid_project", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}; },
        [&](const show_open_faust_file_dialog &) { FileDialog = {"Choose file", FaustDspFileExtension, ".", ""}; },
        [&](const show_save_faust_file_dialog &) { FileDialog = {"Choose file", FaustDspFileExtension, ".", "my_dsp", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}; },
        [&](const show_save_faust_svg_file_dialog &) { FileDialog = {"Choose directory", ".*", ".", "faust_diagram", true, 1, ImGuiFileDialogFlags_ConfirmOverwrite}; },

        [&](const open_file_dialog &a) { FileDialog = a.dialog; },
        [&](const close_file_dialog &) { FileDialog.Visible = false; },

        [&](const set_imgui_settings &a) {
            ImGuiSettings = a.settings;
        },
        [&](const set_imgui_color_style &a) {
            switch (a.id) {
                case 0: Style.ImGui.ColorsDark();
                    break;
                case 1: Style.ImGui.ColorsLight();
                    break;
                case 2: Style.ImGui.ColorsClassic();
                    break;
            }
        },
        [&](const set_implot_color_style &a) {
            switch (a.id) {
                case 0: Style.ImPlot.ColorsAuto();
                    break;
                case 1: Style.ImPlot.ColorsDark();
                    break;
                case 2: Style.ImPlot.ColorsLight();
                    break;
                case 3: Style.ImPlot.ColorsClassic();
                    break;
            }
        },
        [&](const set_flowgrid_color_style &a) {
            switch (a.id) {
                case 0: Style.FlowGrid.ColorsDark();
                    break;
                case 1: Style.FlowGrid.ColorsLight();
                    break;
                case 2: Style.FlowGrid.ColorsClassic();
                    break;
            }
        },
        [&](const set_flowgrid_diagram_color_style &a) {
            switch (a.id) {
                case 0: Style.FlowGrid.DiagramColorsDark();
                    break;
                case 1: Style.FlowGrid.DiagramColorsLight();
                    break;
                case 2: Style.FlowGrid.DiagramColorsClassic();
                    break;
                case 3: Style.FlowGrid.DiagramColorsFaust();
                    break;
            }
        },
        [&](const set_flowgrid_diagram_layout_style &a) {
            switch (a.id) {
                case 0: Style.FlowGrid.DiagramLayoutFlowGrid();
                    break;
                case 1: Style.FlowGrid.DiagramLayoutFaust();
                    break;
            }
        },

        [&](const open_faust_file &a) { Audio.Faust.Code = FileIO::read(a.path); },

        [&](const close_application &) {
            Processes.UI.Running = false;
            Audio.Running = false;
        },

        [&](const auto &) {}, // All actions that don't directly update state (e.g. undo/redo & open/load-project)
    }, action);
}

void save_box_svg(const string &path); // defined in FaustUI

void Context::on_action(const Action &action) {
    if (!action_allowed(action)) return; // Safeguard against actions running in an invalid state.

    std::visit(visitor{
        // Handle actions that don't directly update state.
        // These options don't get added to the action/gesture history, since they only have non-application side effects,
        // and we don't want them replayed when loading a saved `.fga` project.
        [&](const Actions::open_project &a) { open_project(a.path); },
        [&](const open_empty_project &) { open_project(EmptyProjectPath); },
        [&](const open_default_project &) { open_project(DefaultProjectPath); },

        [&](const Actions::save_project &a) { save_project(a.path); },
        [&](const save_default_project &) { save_project(DefaultProjectPath); },
        [&](const Actions::save_current_project &) { save_project(current_project_path.value()); },
        [&](const save_faust_file &a) { FileIO::write(a.path, s.Audio.Faust.Code); },
        [&](const save_faust_svg_file &a) { save_box_svg(a.path); },

        // `diff_index`-changing actions:
        [&](const undo &) { increment_diff_index(-1); },
        [&](const redo &) { increment_diff_index(1); },
        [&](const Actions::set_diff_index &a) {
            if (!active_gesture_patch.empty()) finalize_gesture(); // Make sure any pending actions/diffs are committed.
            set_diff_index(a.diff_index);
        },

        // Remaining actions have a direct effect on the application state.
        // Keep JSON & struct versions of state in sync.
        [&](const set_value &a) {
            const auto before_json = state_json;
            state_json[a.path] = a.value;
            state = state_json;
            on_patch(a, json::diff(before_json, state_json));
        },
        [&](const set_values &a) {
            const auto before_json = state_json;
            for (const auto &[path, value]: a.values) {
                state_json[path] = value;
            }
            state = state_json;
            on_patch(a, json::diff(before_json, state_json));
        },
        [&](const toggle_value &a) {
            const auto before_json = state_json;
            state_json[a.path] = !state_json[a.path];
            state = state_json;
            on_patch(a, json::diff(before_json, state_json));
            // Treat all toggles as immediate actions. Otherwise, performing two toggles in a row and undoing does nothing, since they're compressed into nothing.
            finalize_gesture();
        },
        [&](const auto &a) {
            const auto before_json = state_json;
            state.Update(a);
            state_json = state;
            on_patch(a, json::diff(before_json, state_json));
        },
    }, action);
}
