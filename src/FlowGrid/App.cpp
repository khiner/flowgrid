#include "App.h"

#include <immer/algorithm.hpp>
#include <range/v3/view/filter.hpp>

#include "Helper/File.h"
#include "Helper/String.h"

vector<ImVec4> fg::Style::ImGuiStyle::ColorPresetBuffer(ImGuiCol_COUNT);
vector<ImVec4> fg::Style::ImPlotStyle::ColorPresetBuffer(ImPlotCol_COUNT);

Base::Base(StateMember *parent, string_view path_segment, string_view name_help) : UIStateMember(parent, path_segment, name_help) {
    WithPath[Path] = this;
}
Base::~Base() {
    WithPath.erase(Path);
}

UInt::UInt(StateMember *parent, string_view path_segment, string_view name_help, U32 value, U32 min, U32 max)
    : TypedBase(parent, path_segment, name_help, value), Min(min), Max(max) {}
UInt::UInt(StateMember *parent, string_view path_segment, string_view name_help, std::function<const string(U32)> get_name, U32 value)
    : TypedBase(parent, path_segment, name_help, value), Min(0), Max(100), GetName(std::move(get_name)) {}
UInt::operator bool() const { return Value; }
UInt::operator int() const { return Value; }
UInt::operator ImColor() const { return Value; }
string UInt::ValueName(const U32 value) const { return GetName ? (*GetName)(value) : to_string(value); }

Int::Int(StateMember *parent, string_view path_segment, string_view name_help, int value, int min, int max)
    : TypedBase(parent, path_segment, name_help, value), Min(min), Max(max) {}
Int::operator bool() const { return Value; }
Int::operator short() const { return Value; }
Int::operator char() const { return Value; }
Int::operator S8() const { return Value; }

Float::Float(StateMember *parent, string_view path_segment, string_view name_help, float value, float min, float max, const char *fmt, ImGuiSliderFlags flags, float drag_speed)
    : TypedBase(parent, path_segment, name_help, value), Min(min), Max(max), DragSpeed(drag_speed), Format(fmt), Flags(flags) {}

// todo instead of overriding `Update` to handle ints, try ensuring floats are written to the store.
void Float::Update() {
    const Primitive PrimitiveValue = Get();
    if (std::holds_alternative<int>(PrimitiveValue)) Value = float(std::get<int>(PrimitiveValue));
    else Value = std::get<float>(PrimitiveValue);
}

String::String(StateMember *parent, string_view path_segment, string_view name_help, string_view value)
    : TypedBase(parent, path_segment, name_help, string(value)) {}
String::operator bool() const { return !Value.empty(); }
String::operator string_view() const { return Value; }

Enum::Enum(StateMember *parent, string_view path_segment, string_view name_help, vector<string> names, int value)
    : TypedBase(parent, path_segment, name_help, value), Names(std::move(names)) {}
Enum::Enum(StateMember *parent, string_view path_segment, string_view name_help, std::function<const string(int)> get_name, int value)
    : TypedBase(parent, path_segment, name_help, value), Names({}), GetName(std::move(get_name)) {}
string Enum::OptionName(const int option) const { return GetName ? (*GetName)(option) : Names[option]; }

Flags::Flags(StateMember *parent, string_view path_segment, string_view name_help, vector<Item> items, int value)
    : TypedBase(parent, path_segment, name_help, value), Items(std::move(items)) {}

Flags::Item::Item(const char *name_and_help) {
    const auto &[name, help] = StringHelper::ParseHelpText(name_and_help);
    Name = name;
    Help = help;
}

// Transient modifiers
void Set(const Base &field, const Primitive &value, TransientStore &store) { store.set(field.Path, value); }
void Set(const StoreEntries &values, TransientStore &store) {
    for (const auto &[path, value] : values) store.set(path, value);
}
void Set(const FieldEntries &values, TransientStore &store) {
    for (const auto &[field, value] : values) store.set(field.Path, value);
}
void Set(const StatePath &path, const vector<Primitive> &values, TransientStore &store) {
    Count i = 0;
    while (i < values.size()) {
        store.set(path / to_string(i), values[i]);
        i++;
    }

    while (store.count(path / to_string(i))) store.erase(path / to_string(i++));
}
void Set(const StatePath &path, const vector<Primitive> &data, const Count row_count, TransientStore &store) {
    assert(data.size() % row_count == 0);
    const Count col_count = data.size() / row_count;
    Count row = 0;
    while (row < row_count) {
        Count col = 0;
        while (col < col_count) {
            store.set(path / to_string(row) / to_string(col), data[row * col_count + col]);
            col++;
        }
        while (store.count(path / to_string(row) / to_string(col))) store.erase(path / to_string(row) / to_string(col++));
        row++;
    }

    while (store.count(path / to_string(row) / to_string(0))) {
        Count col = 0;
        while (store.count(path / to_string(row) / to_string(col))) store.erase(path / to_string(row) / to_string(col++));
        row++;
    }
}

Patch CreatePatch(const Store &before, const Store &after, const StatePath &BasePath) {
    PatchOps ops{};
    diff(
        before,
        after,
        [&](auto const &added_element) {
            ops[added_element.first.lexically_relative(BasePath)] = {AddOp, added_element.second, {}};
        },
        [&](auto const &removed_element) {
            ops[removed_element.first.lexically_relative(BasePath)] = {RemoveOp, {}, removed_element.second};
        },
        [&](auto const &old_element, auto const &new_element) {
            ops[old_element.first.lexically_relative(BasePath)] = {ReplaceOp, new_element.second, old_element.second};
        }
    );

    return {ops, BasePath};
}

string to_string(const IO io, const bool shorten) {
    switch (io) {
        case IO_In: return shorten ? "in" : "input";
        case IO_Out: return shorten ? "out" : "output";
        case IO_None: return "none";
    }
}
string to_string(PatchOp::Type patch_op_type) {
    switch (patch_op_type) {
        case AddOp: return "Add";
        case RemoveOp: return "Remove";
        case ReplaceOp: return "Replace";
    }
}

namespace action {

#define ActionName(action_var_name) StringHelper::PascalToSentenceCase(#action_var_name)

string GetName(const ProjectAction &action) {
    return Match(
        action,
        [](const Undo &) { return ActionName(Undo); },
        [](const Redo &) { return ActionName(Redo); },
        [](const SetHistoryIndex &) { return ActionName(SetHistoryIndex); },
        [](const OpenProject &) { return ActionName(OpenProject); },
        [](const OpenEmptyProject &) { return ActionName(OpenEmptyProject); },
        [](const OpenDefaultProject &) { return ActionName(OpenDefaultProject); },
        [](const SaveProject &) { return ActionName(SaveProject); },
        [](const SaveDefaultProject &) { return ActionName(SaveDefaultProject); },
        [](const SaveCurrentProject &) { return ActionName(SaveCurrentProject); },
        [](const SaveFaustFile &) { return "Save Faust file"s; },
        [](const SaveFaustSvgFile &) { return "Save Faust SVG file"s; },
    );
}

string GetName(const StateAction &action) {
    return Match(
        action,
        [](const OpenFaustFile &) { return "Open Faust file"s; },
        [](const ShowOpenFaustFileDialog &) { return "Show open Faust file dialog"s; },
        [](const ShowSaveFaustFileDialog &) { return "Show save Faust file dialog"s; },
        [](const ShowSaveFaustSvgFileDialog &) { return "Show save Faust SVG file dialog"s; },
        [](const SetImGuiColorStyle &) { return "Set ImGui color style"s; },
        [](const SetImPlotColorStyle &) { return "Set ImPlot color style"s; },
        [](const SetFlowGridColorStyle &) { return "Set FlowGrid color style"s; },
        [](const SetGraphColorStyle &) { return ActionName(SetGraphColorStyle); },
        [](const SetGraphLayoutStyle &) { return ActionName(SetGraphLayoutStyle); },
        [](const OpenFileDialog &) { return ActionName(OpenFileDialog); },
        [](const CloseFileDialog &) { return ActionName(CloseFileDialog); },
        [](const ShowOpenProjectDialog &) { return ActionName(ShowOpenProjectDialog); },
        [](const ShowSaveProjectDialog &) { return ActionName(ShowSaveProjectDialog); },
        [](const SetValue &) { return ActionName(SetValue); },
        [](const SetValues &) { return ActionName(SetValues); },
        [](const SetVector &) { return ActionName(SetVector); },
        [](const SetMatrix &) { return ActionName(SetMatrix); },
        [](const ToggleValue &) { return ActionName(ToggleValue); },
        [](const ApplyPatch &) { return ActionName(ApplyPatch); },
        [](const CloseApplication &) { return ActionName(CloseApplication); },
    );
}

string GetShortcut(const EmptyAction &action) {
    const ID id = std::visit([](const Action &&a) { return GetId(a); }, action);
    return ShortcutForId.contains(id) ? ShortcutForId.at(id) : "";
}

// An action's menu label is its name, except for a few exceptions.
string GetMenuLabel(const EmptyAction &action) {
    return Match(
        action,
        [](const ShowOpenProjectDialog &) { return "Open project"s; },
        [](const OpenEmptyProject &) { return "New project"s; },
        [](const SaveCurrentProject &) { return "Save project"s; },
        [](const ShowSaveProjectDialog &) { return "Save project as..."s; },
        [](const ShowOpenFaustFileDialog &) { return "Open DSP file"s; },
        [](const ShowSaveFaustFileDialog &) { return "Save DSP as..."s; },
        [](const ShowSaveFaustSvgFileDialog &) { return "Export SVG"s; },
        [](const ProjectAction &a) { return GetName(a); },
        [](const StateAction &a) { return GetName(a); },
    );
}
} // namespace action

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
