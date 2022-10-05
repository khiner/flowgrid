#pragma once

#include "State.h"

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

JsonType(Style::ImGuiStyleMember, Alpha, DisabledAlpha, WindowPadding, WindowRounding, WindowBorderSize, WindowMinSize, WindowTitleAlign, WindowMenuButtonPosition, ChildRounding, ChildBorderSize, PopupRounding,
    PopupBorderSize, FramePadding, FrameRounding, FrameBorderSize, ItemSpacing, ItemInnerSpacing, CellPadding, TouchExtraPadding, IndentSpacing, ColumnsMinSpacing, ScrollbarSize, ScrollbarRounding, GrabMinSize,
    GrabRounding, LogSliderDeadzone, TabRounding, TabBorderSize, TabMinWidthForCloseButton, ColorButtonPosition, ButtonTextAlign, SelectableTextAlign, DisplayWindowPadding, DisplaySafeAreaPadding, MouseCursorScale,
    AntiAliasedLines, AntiAliasedLinesUseTex, AntiAliasedFill, CurveTessellationTol, CircleTessellationMaxError, Colors)
JsonType(Style::ImPlotStyleMember, LineWeight, Marker, MarkerSize, MarkerWeight, FillAlpha, ErrorBarSize, ErrorBarWeight, DigitalBitHeight, DigitalBitGap, PlotBorderSize, MinorAlpha, MajorTickLen, MinorTickLen,
    MajorTickSize, MinorTickSize, MajorGridSize, MinorGridSize, PlotPadding, LabelPadding, LegendPadding, LegendInnerPadding, LegendSpacing, MousePosPadding, AnnotationPadding, FitPadding, PlotDefaultSize, PlotMinSize,
    Colors, Colormap, UseLocalTime, UseISO8601, Use24HourClock)
JsonType(FlowGridStyle, Colors, FlashDurationSec,
    DiagramFoldComplexity, DiagramDirection, DiagramSequentialConnectionZigzag, DiagramOrientationMark, DiagramOrientationMarkRadius, DiagramRouteFrame, DiagramScaleLinked,
    DiagramScaleFill, DiagramScale, DiagramTopLevelMargin, DiagramDecorateMargin, DiagramDecorateLineWidth, DiagramDecorateCornerRadius, DiagramBoxCornerRadius, DiagramBinaryHorizontalGapRatio, DiagramWireGap,
    DiagramGap, DiagramWireWidth, DiagramArrowSize, DiagramInverterRadius,
    ParamsHeaderTitles, ParamsAlignmentHorizontal, ParamsAlignmentVertical, ParamsTableFlags, ParamsTableSizingPolicy)
JsonType(Style, Visible, ImGui, ImPlot, FlowGrid)

// Double-check occasionally that the fields in these ImGui settings definitions still match their ImGui counterparts.
JsonType(ImGuiDockNodeSettings, ID, ParentNodeId, ParentWindowId, SelectedTabId, SplitAxis, Depth, Flags, Pos, Size, SizeRef)
JsonType(ImGuiWindowSettings, ID, Pos, Size, ViewportPos, ViewportId, DockId, ClassId, DockOrder, Collapsed)
JsonType(ImGuiTableSettings, ID, SaveFlags, RefScale, ColumnsCount, ColumnsCountMax)
JsonType(TableColumnSettings, WidthOrWeight, UserID, Index, DisplayOrder, SortOrder, SortDirection, IsEnabled, IsStretch)
JsonType(TableSettings, Table, Columns)
JsonType(ImGuiSettingsData, Nodes, Windows, Tables)
JsonType(Processes, UI)
JsonType(StateData, ApplicationSettings, Audio, File, Style, ImGuiSettings, Processes, StateViewer, StateMemoryEditor, PathUpdateFrequency, ProjectPreview, Demo, Metrics, Tools);

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
