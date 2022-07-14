#pragma once

#include <set>
#include <utility>
#include "range/v3/view.hpp"

#include "StateData.h"
#include "JsonType.h"
#include "Action.h"

/**
 * The entire codebase has read-only access to the immutable, single source-of-truth application state instance `s`.
 * The global `Context c` instance updates `s` when it receives an `Action a` instance via the global `BlockingConcurrentQueue<Action> q` action queue.
 *
  * `{Stateful}` structs extend their data-only `{Stateful}Data` parents, adding derived (and always present) fields for commonly accessed,
 *   but expensive-to-compute derivations of their core (minimal but complete) data members.
 *  Many `{Stateful}` structs also implement convenience methods for complex state updates across multiple fields,
 *    or for generating less-frequently needed derived data.
 *
 * **The global `const StateData &s` instance is declared in `context.h` and instantiated in `main.cpp`.**
 */

using Action = action::Action;
using std::string;

enum ProjectFormat {
    None,
    StateFormat,
    DiffFormat,
};

const std::map<ProjectFormat, string> ExtensionForProjectFormat{
    {StateFormat, ".fls"},
    {DiffFormat,  ".fld"},
};
const std::map<string, ProjectFormat> ProjectFormatForExtension{
    {ExtensionForProjectFormat.at(StateFormat), StateFormat},
    {ExtensionForProjectFormat.at(DiffFormat),  DiffFormat},
};

static const std::set<string> AllProjectExtensions = {".fls", ".fld"};
static const string AllProjectExtensionsDelimited = AllProjectExtensions | ranges::views::join(',') | ranges::to<std::string>();
static const string PreferencesFileExtension = ".flp";
static const string FaustDspFileExtension = ".dsp";

static const fs::path InternalPath = ".flowgrid";
static const fs::path EmptyProjectPath = InternalPath / ("empty" + ExtensionForProjectFormat.at(StateFormat));
static const fs::path DefaultProjectPath = InternalPath / ("default" + ExtensionForProjectFormat.at(StateFormat));
static const fs::path PreferencesPath = InternalPath / ("preferences" + PreferencesFileExtension);

// E.g. `convert_state_path("s.foo.bar") => "/foo/bar"`
// TODO return `fs::path`
inline std::string convert_state_path(const string &state_member_name) {
    std::size_t index = state_member_name.find('.');
    // Must pass the fully-qualified state path.
    // Could also check that the first segment is `s`, `_s`, `state`, or `_state`, but that's expensive and
    assert(index != std::string::npos);
    std::string subpath = state_member_name.substr(index);
    std::replace(subpath.begin(), subpath.end(), '.', '/');
    return subpath;
}

// Used to convert a state variable member to its respective path in state JSON.
// This allows for code paths conditioned on which state member was changed, without needing to worry about manually changing paths when state members move.
// _Must pass the fully qualified, nested state variable, starting with the root state variable (e.g. `s` or `state`).
// E.g. `StatePath(s.foo.bar) => "/foo/bar"`
#define StatePath(x) convert_state_path(#x)

struct Audio : StateData::Audio, Drawable {
    void draw() const override;

    struct Settings : StateData::Audio::Settings, Drawable {
        Settings() : StateData::Audio::Settings() {};
        void draw() const override;
    };

    struct Faust {
        struct Editor : StateData::Audio::Faust, Drawable {
            Editor() : StateData::Audio::Faust() {}
            void draw() const override;
        };

        struct Log : StateData::Audio::Faust::Log, Drawable {
            Log() : StateData::Audio::Faust::Log() {}
            void draw() const override;
        };
    };

    Settings settings{};
    Faust faust{};
};

struct Style : StateData::Style, Drawable {
    Style() { name = "Style"; }
    void draw() const override;

private:
    // `...StyleEditor` methods are drawn as tabs, and return `true` if style changes.
    static bool ImGuiStyleEditor();
    static bool ImPlotStyleEditor();
    static bool FlowGridStyleEditor();
};

struct State : StateData {
    State() = default;
    // Don't copy/assign reference members!
    explicit State(const StateData &other) : StateData(other) {}

    State &operator=(const State &other) {
        StateData::operator=(other);
        return *this;
    }

    void update(const Action &); // State is only updated via `context.on_action(action)`

    std::vector<std::reference_wrapper<WindowData>> all_windows{
        windows.state_viewer, windows.memory_editor, windows.path_update_frequency,
        style, windows.demo, windows.metrics, windows.tools,
        audio.settings, audio.faust.editor, audio.faust.log
    };
    std::vector<std::reference_wrapper<const WindowData>> all_windows_const{
        windows.state_viewer, windows.memory_editor, windows.path_update_frequency,
        style, windows.demo, windows.metrics, windows.tools,
        audio.settings, audio.faust.editor, audio.faust.log
    };

    WindowData &named(const string &name) {
        for (auto &window: all_windows) {
            if (name == window.get().name) return window;
        }
        throw std::invalid_argument(name);
    }

    const WindowData &named(const string &name) const {
        for (auto &window: all_windows_const) {
            if (name == window.get().name) return window;
        }
        throw std::invalid_argument(name);
    }
};

// Types for [json-patch](https://jsonpatch.com)
// For a much more well-defined schema, see https://json.schemastore.org/json-patch
// A JSON-schema validation lib like https://github.com/tristanpenman/valijson

enum JsonPatchOpType {
    Add,
    Remove,
    Replace,
    Copy,
    Move,
    Test,
};
struct JsonPatchOp {
    string path;
    JsonPatchOpType op{};
    std::optional<json> value; // Present for add/replace/test
    std::optional<string> from; // Present for copy/move
};
using JsonPatch = std::vector<JsonPatchOp>;

// One issue with this data structure is that forward & reverse diffs both redundantly store the same json path(s).
struct BidirectionalStateDiff {
    std::set<string> action_names;
    JsonPatch forward_patch;
    JsonPatch reverse_patch;
    TimePoint system_time;
};

NLOHMANN_JSON_SERIALIZE_ENUM(JsonPatchOpType, {
    { Add, "add" },
    { Remove, "remove" },
    { Replace, "replace" },
    { Copy, "copy" },
    { Move, "move" },
    { Test, "test" },
})

JsonType(JsonPatchOp, path, op, value)
JsonType(BidirectionalStateDiff, action_names, forward_patch, reverse_patch, system_time)

JsonType(ImVec2, x, y)
JsonType(ImVec4, w, x, y, z)
JsonType(ImVec2ih, x, y)

JsonType(WindowData, name, visible)

JsonType(Audio::Faust::Editor, name, visible, file_name)
JsonType(Audio::Faust, code, error, editor, log)
JsonType(Audio::Settings, name, visible, muted, backend, latency, sample_rate, out_raw)
JsonType(Audio, settings, faust)
JsonType(File::Dialog, visible, title, save_mode, filters, path, default_file_name, max_num_selections, flags)
JsonType(File, dialog)
JsonType(Windows::StateViewer, name, visible, label_mode, auto_select)
JsonType(Windows, state_viewer, memory_editor, path_update_frequency, demo, metrics, tools)

JsonType(ImGuiStyle, Alpha, DisabledAlpha, WindowPadding, WindowRounding, WindowBorderSize, WindowMinSize, WindowTitleAlign, WindowMenuButtonPosition, ChildRounding, ChildBorderSize, PopupRounding, PopupBorderSize,
    FramePadding, FrameRounding, FrameBorderSize, ItemSpacing, ItemInnerSpacing, CellPadding, TouchExtraPadding, IndentSpacing, ColumnsMinSpacing, ScrollbarSize, ScrollbarRounding, GrabMinSize, GrabRounding,
    LogSliderDeadzone, TabRounding, TabBorderSize, TabMinWidthForCloseButton, ColorButtonPosition, ButtonTextAlign, SelectableTextAlign, DisplayWindowPadding, DisplaySafeAreaPadding, MouseCursorScale, AntiAliasedLines,
    AntiAliasedLinesUseTex, AntiAliasedFill, CurveTessellationTol, CircleTessellationMaxError, Colors)
JsonType(ImPlotStyle, LineWeight, Marker, MarkerSize, MarkerWeight, FillAlpha, ErrorBarSize, ErrorBarWeight, DigitalBitHeight, DigitalBitGap, PlotBorderSize, MinorAlpha, MajorTickLen, MinorTickLen, MajorTickSize,
    MinorTickSize, MajorGridSize, MinorGridSize, PlotPadding, LabelPadding, LegendPadding, LegendInnerPadding, LegendSpacing, MousePosPadding, AnnotationPadding, FitPadding, PlotDefaultSize, PlotMinSize, Colors,
    Colormap, UseLocalTime, UseISO8601, Use24HourClock)
JsonType(FlowGridStyle, Colors, FlashDurationSec)
JsonType(Style, name, visible, imgui, implot, flowgrid)

// Double-check occasionally that the fields in these ImGui settings definitions still match their ImGui counterparts.
JsonType(ImGuiDockNodeSettings, ID, ParentNodeId, ParentWindowId, SelectedTabId, SplitAxis, Depth, Flags, Pos, Size, SizeRef)
JsonType(ImGuiWindowSettings, ID, Pos, Size, ViewportPos, ViewportId, DockId, ClassId, DockOrder, Collapsed)
JsonType(ImGuiTableSettings, ID, SaveFlags, RefScale, ColumnsCount, ColumnsCountMax)
JsonType(ImGuiSettings, nodes, windows, tables)

JsonType(Processes::Process, running)
JsonType(Processes, audio, ui)

JsonType(StateData, audio, file, style, imgui_settings, processes, windows);
