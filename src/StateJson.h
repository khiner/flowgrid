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

// Serialize variants.
// Based on https://github.com/nlohmann/json/issues/1261#issuecomment-426209912
// todo should be able to simplify the switch part.
namespace detail {
template<std::size_t N>
struct variant_switch {
    template<typename Variant>
    constexpr void operator()(int index, const json &value, Variant &v) const {
        if (index == N) v = value.get<std::variant_alternative_t<N, Variant>>();
        else variant_switch<N - 1>{}(index, value, v);
    }
};

template<>
struct variant_switch<0> {
    template<typename Variant>
    constexpr void operator()(int index, const json &value, Variant &v) const {
        if (index == 0) v = value.get<std::variant_alternative_t<0, Variant>>();
        else throw std::runtime_error("while converting json to variant: invalid index");
    }
};
}

namespace nlohmann {
template<typename ...Args>
// Serialize variants as two-element arrays, [index, value]. Value element can possibly be null.
struct adl_serializer<std::variant<Args...>> {
    static constexpr inline void to_json(json &j, const std::variant<Args...> &v) {
        std::visit([&](auto &&value) {
            j = {v.index(), std::forward<decltype(value)>(value)};
        }, v);
    }

    static constexpr inline void from_json(const json &j, std::variant<Args...> &v) {
        ::detail::variant_switch<sizeof...(Args) - 1>{}(j[0].get<int>(), j[1], v);
    }
};
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
JsonType(ImVec4, w, x, y, z)
JsonType(ImVec2ih, x, y)

namespace nlohmann {
inline void to_json(json &j, const Bool &field) { j = field.value; }
inline void from_json(const json &j, Bool &field) { field.value = j; }

inline void to_json(json &j, const Float &field) { j = field.value; }
inline void from_json(const json &j, Float &field) { field.value = j; }

inline void to_json(json &j, const Vec2 &field) { j = field.value; }
inline void from_json(const json &j, Vec2 &field) { field.value = j; }

inline void to_json(json &j, const Int &field) { j = field.value; }
inline void from_json(const json &j, Int &field) { field.value = j; }

inline void to_json(json &j, const String &field) { j = field.value; }
inline void from_json(const json &j, String &field) { field.value = j; }

inline void to_json(json &j, const Enum &field) { j = field.value; }
inline void from_json(const json &j, Enum &field) { field.value = j; }

inline void to_json(json &j, const Flags &field) { j = field.value; }
inline void from_json(const json &j, Flags &field) { field.value = j; }
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
JsonType(BidirectionalStateDiff, Forward, Reverse, Time)

JsonType(Window, Visible)
JsonType(Process, Running)

JsonType(ApplicationSettings, Visible, GestureDurationSec)
JsonType(Audio::FaustState::FaustEditor, Visible, FileName)
JsonType(Audio::FaustState::FaustDiagram::DiagramSettings, HoverFlags)
JsonType(Audio::FaustState::FaustDiagram, Visible, Settings)
JsonType(Audio::FaustState::FaustParams, Visible)
JsonType(Audio::FaustState, Code, Diagram, Params, Error, Editor, Log)
JsonType(Audio, Visible, Running, FaustRunning, InDeviceId, OutDeviceId, InSampleRate, OutSampleRate, InFormat, OutFormat, OutDeviceVolume, Muted, Backend, MonitorInput, Faust)
JsonType(File::FileDialog, Visible, Title, SaveMode, Filters, FilePath, DefaultFileName, MaxNumSelections, Flags) // todo without this, error "type must be string, but is object" on project load
JsonType(File::DialogData, Visible, Title, SaveMode, Filters, FilePath, DefaultFileName, MaxNumSelections, Flags)
JsonType(File, Dialog)
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
JsonType(State, ApplicationSettings, Audio, File, Style, ImGuiSettings, Processes, StateViewer, StateMemoryEditor, PathUpdateFrequency, ProjectPreview, Demo, Info, Metrics, StackTool, DebugLog);

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

JsonType(set_diff_index, diff_index)
JsonType(open_project, path)
JsonType(open_file_dialog, dialog)
JsonType(save_project, path)
JsonType(set_value, path, value)
JsonType(set_values, values)
//JsonType(patch_value, patch)
JsonType(toggle_value, path)
JsonType(set_imgui_color_style, id)
JsonType(set_implot_color_style, id)
JsonType(set_flowgrid_color_style, id)
JsonType(set_flowgrid_diagram_color_style, id)
JsonType(set_flowgrid_diagram_layout_style, id)
JsonType(save_faust_file, path)
JsonType(open_faust_file, path)
JsonType(save_faust_svg_file, path)
} // End `Action` namespace
