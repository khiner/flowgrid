#pragma once

#include <variant>

#include "App.h"

namespace nlohmann {
inline void to_json(json &j, const JsonPath &path) { j = path.to_string(); }
inline void from_json(const json &j, JsonPath &path) { path = JsonPath(j.get<std::string>()); }

// Convert `std::chrono::time_point`s to/from JSON.
// From https://github.com/nlohmann/json/issues/2159#issuecomment-638104529
template<typename Clock, typename Duration>
struct adl_serializer<std::chrono::time_point<Clock, Duration>> {
    static constexpr inline void to_json(json &j, const std::chrono::time_point<Clock, Duration> &tp) {
        j = tp.time_since_epoch().count();
    }
    static constexpr inline void from_json(const json &j, std::chrono::time_point<Clock, Duration> &tp) {
        Duration duration(j);
        tp = std::chrono::time_point<Clock, Duration>{duration};
    }
};
}

// This boilerplate is for handling `std::optional` values.
// From https://github.com/nlohmann/json/issues/1749#issuecomment-1099890282
template<class J, class T>
constexpr void optional_to_json(J &j, const char *name, const std::optional<T> &value) {
    if (value) j[name] = *value;
}
template<class J, class T>
constexpr void optional_from_json(const J &j, const char *name, std::optional<T> &value) {
    const auto it = j.find(name);
    if (it != j.end()) value = it->template get<T>();
    else value = nullopt;
}

template<typename>
constexpr bool is_optional = false;
template<typename T>
constexpr bool is_optional<std::optional<T>> = true;

template<typename T>
constexpr void extended_to_json(const char *key, json &j, const T &value) {
    if constexpr (is_optional<T>) optional_to_json(j, key, value);
    else j[key] = value;
}
template<typename T>
constexpr void extended_from_json(const char *key, const json &j, T &value) {
    if constexpr (is_optional<T>) optional_from_json(j, key, value);
    else j.at(key).get_to(value);
}

namespace nlohmann {
inline void to_json(json &j, const ImVec4 &value) {
    j = json{{"x", value.x}, {"y", value.y}, {"z", value.z}, {"w", value.w}};
}
inline void from_json(const json &j, ImVec4 &field) {
    const json &inner_j = j.is_array() ? j[1] : j;
    field = {inner_j["x"], inner_j["y"], inner_j["z"], inner_j["w"]};
}
}

#define ExtendedToJson(v1) extended_to_json(#v1, nlohmann_json_j, nlohmann_json_t.v1);
#define ExtendedFromJson(v1) extended_from_json(#v1, nlohmann_json_j, nlohmann_json_t.v1);

#define JsonType(Type, ...) \
    constexpr inline void to_json(nlohmann::json &nlohmann_json_j, const Type &nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(ExtendedToJson, __VA_ARGS__)) } \
    constexpr inline void from_json(const nlohmann::json &nlohmann_json_j, Type &nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(ExtendedFromJson, __VA_ARGS__)) }
#define EmptyJsonType(Type) \
    constexpr inline void to_json(nlohmann::json &, const Type &) {} \
    constexpr inline void from_json(const nlohmann::json &, Type &) {}

JsonType(Preferences, recently_opened_paths)

JsonType(ImVec2, x, y)
JsonType(ImVec2ih, x, y)

namespace nlohmann {
// Serialize actions as two-element arrays, [index, value]. Value element can possibly be null.
inline void to_json(json &j, const Action &v) {
    std::visit([&](auto &&value) {
        j = {v.index(), std::forward<decltype(value)>(value)};
    }, v);
}

inline void from_json(const json &j, Action &v) {
    v = action::create(j[0].get<int>()); // todo fill values
}

inline void to_json(json &j, const Primitive &field) {
    std::visit([&](auto &&value) {
        j = std::forward<decltype(value)>(value);
    }, field);
}
inline void from_json(const json &j, Primitive &field) {
    field = j;
}
inline void to_json(json &j, const Bool &field) { j = bool(field); }
inline void from_json(const json &j, Bool &field) { field = bool(j); }

inline void to_json(json &j, const Float &field) { j = float(field); }
inline void from_json(const json &j, Float &field) { field = float(j); }

inline void to_json(json &j, const Vec2 &field) { j = ImVec2(field); }
inline void from_json(const json &j, Vec2 &field) { field = ImVec2(j); }

inline void to_json(json &j, const Int &field) { j = int(field); }
inline void from_json(const json &j, Int &field) { field = int(j); }

inline void to_json(json &j, const String &field) { j = string(field); }
inline void from_json(const json &j, String &field) { field = string(j); }

inline void to_json(json &j, const Enum &field) { j = int(field); }
inline void from_json(const json &j, Enum &field) { field = int(j); }

inline void to_json(json &j, const Flags &field) { j = int(field); }
inline void from_json(const json &j, Flags &field) { field = int(j); }

inline void to_json(json &j, const Colors &field) {
    vector<ImVec4> vec(field.size());
    for (int i = 0; i < int(field.size()); i++) { vec[i] = field[i]; }
    j = vec;
}
inline void from_json(const json &j, Colors &field) { field = vector<ImVec4>(j); }
}

NLOHMANN_JSON_SERIALIZE_ENUM(JsonPatchOpType, {
    { Add, "add" },
    { Remove, "remove" },
    { Replace, "replace" },
    { Copy, "copy" },
    { Move, "move" },
    { Test, "test" },
})

JsonType(JsonPatchOp, path, op, value, from) // lower-case since these are deserialized and passed directly to json-lib.
JsonType(StatePatch, Patch, Time)

JsonType(Window, Visible)
JsonType(Process, Running)

JsonType(ApplicationSettings, Visible, GestureDurationSec)
JsonType(Audio::FaustState::FaustEditor, Visible, FileName)
JsonType(Audio::FaustState::FaustDiagram::DiagramSettings, HoverFlags)
JsonType(Audio::FaustState::FaustDiagram, Visible, Settings)
JsonType(Audio::FaustState::FaustParams, Visible)
JsonType(Audio::FaustState, Code, Diagram, Params, Error, Editor, Log)
JsonType(Audio, Visible, Running, FaustRunning, InDeviceId, OutDeviceId, InSampleRate, OutSampleRate, InFormat, OutFormat, OutDeviceVolume, Muted, Backend, MonitorInput, Faust)
JsonType(FileDialog, Visible, Title, SaveMode, Filters, FilePath, DefaultFileName, MaxNumSelections, Flags)
JsonType(StateViewer, Visible, LabelMode, AutoSelect)
JsonType(ProjectPreview, Visible, Format, Raw)
JsonType(Metrics::FlowGridMetrics, ShowRelativePaths)
JsonType(Metrics, Visible, FlowGrid)

JsonType(Style::FlowGridStyle, Colors, FlashDurationSec,
    DiagramFoldComplexity, DiagramDirection, DiagramSequentialConnectionZigzag, DiagramOrientationMark, DiagramOrientationMarkRadius, DiagramRouteFrame, DiagramScaleLinked,
    DiagramScaleFill, DiagramScale, DiagramTopLevelMargin, DiagramDecorateMargin, DiagramDecorateLineWidth, DiagramDecorateCornerRadius, DiagramBoxCornerRadius, DiagramBinaryHorizontalGapRatio, DiagramWireGap,
    DiagramGap, DiagramWireWidth, DiagramArrowSize, DiagramInverterRadius,
    ParamsHeaderTitles, ParamsMinHorizontalItemWidth, ParamsMaxHorizontalItemWidth, ParamsMinVerticalItemHeight, ParamsMinKnobItemSize, ParamsAlignmentHorizontal, ParamsAlignmentVertical, ParamsTableFlags,
    ParamsWidthSizingPolicy)
JsonType(Style::ImGuiStyle, Alpha, DisabledAlpha, WindowPadding, WindowRounding, WindowBorderSize, WindowMinSize, WindowTitleAlign, WindowMenuButtonPosition, ChildRounding, ChildBorderSize, PopupRounding,
    PopupBorderSize, FramePadding, FrameRounding, FrameBorderSize, ItemSpacing, ItemInnerSpacing, CellPadding, TouchExtraPadding, IndentSpacing, ColumnsMinSpacing, ScrollbarSize, ScrollbarRounding, GrabMinSize,
    GrabRounding, LogSliderDeadzone, TabRounding, TabBorderSize, TabMinWidthForCloseButton, ColorButtonPosition, ButtonTextAlign, SelectableTextAlign, DisplayWindowPadding, DisplaySafeAreaPadding, MouseCursorScale,
    AntiAliasedLines, AntiAliasedLinesUseTex, AntiAliasedFill, CurveTessellationTol, CircleTessellationMaxError, FontIndex, FontScale, Colors)
JsonType(Style::ImPlotStyle, LineWeight, Marker, MarkerSize, MarkerWeight, FillAlpha, ErrorBarSize, ErrorBarWeight, DigitalBitHeight, DigitalBitGap, PlotBorderSize, MinorAlpha, MajorTickLen, MinorTickLen,
    MajorTickSize, MinorTickSize, MajorGridSize, MinorGridSize, PlotPadding, LabelPadding, LegendPadding, LegendInnerPadding, LegendSpacing, MousePosPadding, AnnotationPadding, FitPadding, PlotDefaultSize, PlotMinSize,
    Colors, Colormap, UseLocalTime, UseISO8601, Use24HourClock)
JsonType(Style, Visible, FlowGrid, ImGui, ImPlot)

// Double-check occasionally that the fields in these ImGui settings definitions still match their ImGui counterparts.
JsonType(ImGuiDockNodeSettings, ID, ParentNodeId, ParentWindowId, SelectedTabId, SplitAxis, Depth, Flags, Pos, Size, SizeRef)
JsonType(ImGuiWindowSettings, ID, Pos, Size, ViewportPos, ViewportId, DockId, ClassId, DockOrder, Collapsed)
JsonType(ImGuiTableSettings, ID, SaveFlags, RefScale, ColumnsCount, ColumnsCountMax)
JsonType(TableColumnSettings, WidthOrWeight, UserID, Index, DisplayOrder, SortOrder, SortDirection, IsEnabled, IsStretch)
JsonType(TableSettings, Table, Columns)
JsonType(ImGuiSettingsData, Nodes, Windows, Tables)
JsonType(Processes, UI)
JsonType(State, ApplicationSettings, Audio, FileDialog, Style, ImGuiSettings, Processes, StateViewer, StateMemoryEditor, PathUpdateFrequency, ProjectPreview, Demo, Info, Metrics, StackTool, DebugLog);

JsonType(FileDialogData, title, filters, file_path, default_file_name, save_mode, max_num_selections, flags)

namespace Actions {
EmptyJsonType(undo)
EmptyJsonType(redo)
EmptyJsonType(open_empty_project)
EmptyJsonType(open_default_project)
EmptyJsonType(show_open_project_dialog)
EmptyJsonType(close_file_dialog)
EmptyJsonType(save_current_project)
EmptyJsonType(save_default_project)
EmptyJsonType(show_save_project_dialog)
EmptyJsonType(close_application)
EmptyJsonType(show_open_faust_file_dialog)
EmptyJsonType(show_save_faust_file_dialog)
EmptyJsonType(show_save_faust_svg_file_dialog)

JsonType(set_history_index, history_index)
JsonType(open_project, path)
JsonType(open_file_dialog, dialog)
JsonType(save_project, path)
JsonType(set_value, path, value)
JsonType(set_values, values)
//JsonType(patch_value, patch)
JsonType(toggle_value, path)
JsonType(set_imgui_settings, settings)
JsonType(set_imgui_color_style, id)
JsonType(set_implot_color_style, id)
JsonType(set_flowgrid_color_style, id)
JsonType(set_flowgrid_diagram_color_style, id)
JsonType(set_flowgrid_diagram_layout_style, id)
JsonType(save_faust_file, path)
JsonType(open_faust_file, path)
JsonType(save_faust_svg_file, path)
} // End `Action` namespace
