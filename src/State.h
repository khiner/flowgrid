#pragma once

#include <set>
#include <filesystem>

#include "ImGuiFileDialog.h"
#include "UI/UIContext.h"

#include "JsonType.h"
#include "Helper/String.h"

/**
 * `StateData` is a data-only struct which fully describes the application at any point in time.
 *
 * The entire codebase has read-only access to the immutable, single source-of-truth application `State` instance `s`.
 * The global `Context c` instance updates `s` when it receives an `Action` instance via the global `q(Action)` action queue method.
 *
  * `{Stateful}` structs extend their data-only `{Stateful}Data` parents, adding derived (and always present) fields for commonly accessed,
 *   but expensive-to-compute derivations of their core (minimal but complete) data members.
 *  Many `{Stateful}` structs also implement convenience methods for complex state updates across multiple fields,
 *    or for generating less-frequently needed derived data.
 *
 * **The global `const StateData &s` instance is declared in `context.h` and instantiated in `main.cpp`.**
 */

namespace fs = std::filesystem;

// Time declarations inspired by https://stackoverflow.com/a/14391562/780425
using namespace std::chrono_literals; // Support literals like `1s` or `500ms`
using Clock = std::chrono::system_clock; // Main system clock
using DurationSec = float; // floats used for main duration type
using fsec = std::chrono::duration<DurationSec>; // float seconds as a std::chrono::duration
using TimePoint = Clock::time_point;

struct StateMember {
    StateMember(const JsonPath &parent_path, const string &id, const string &name = "")
        : path(parent_path / id), id(id), name(name.empty() ? snake_case_to_sentence_case(id) : name) {}

    JsonPath path; // todo add start byte offset relative to state root, and link from state viewer json nodes to memory editor
    string id, name;
};

struct Drawable {
    virtual void draw() const = 0;
};

struct Process : StateMember {
    using StateMember::StateMember;

    virtual void update_process() const {}; // Start/stop the thread based on the current `running` state, and any other needed housekeeping.

    bool running = true;
};

struct Window : StateMember, Drawable {
    using StateMember::StateMember;
    Window(const JsonPath &parent_path, const string &id, const string &name = "", bool visible = true) : StateMember(parent_path, id, name), visible(visible) {}

    bool visible{true};

    ImGuiWindow &FindImGuiWindow() const { return *ImGui::FindWindowByName(name.c_str()); }
    void DrawWindow(ImGuiWindowFlags flags = ImGuiWindowFlags_None) const;
    void Dock(ImGuiID node_id) const;
    bool ToggleMenuItem() const;
    void SelectTab() const;
};

const DurationSec GestureDurationSecMin = 0.0, GestureDurationSecMax = 5.0;
struct ApplicationSettings : Window {
    using Window::Window;
    void draw() const override;

    DurationSec GestureDurationSec{0.5}; // Merge actions occurring in short succession into a single gesture
};

struct StateViewer : Window {
    using Window::Window;
    void draw() const override;

    enum LabelMode { annotated, raw };
    LabelMode label_mode{annotated};
    bool auto_select{true};
};

struct StateMemoryEditor : Window {
    using Window::Window;
    void draw() const override;
};

struct StatePathUpdateFrequency : Window {
    using Window::Window;
    void draw() const override;
};

enum ProjectFormat { None = 0, StateFormat, DiffFormat, ActionFormat, };
static const string ProjectFormatLabels[] = {"None", "StateFormat", "DiffFormat", "ActionFormat"};

struct ProjectPreview : Window {
    using Window::Window;
    void draw() const override;

    ProjectFormat format = StateFormat;
};

struct Demo : Window {
    using Window::Window;
    void draw() const override;
};

struct Metrics : Window {
    using Window::Window;
    void draw() const override;

    struct FlowGridMetrics : StateMember, Drawable {
        using StateMember::StateMember;
        void draw() const override;
        bool show_relative_paths = true;
    };
    struct ImGuiMetrics : StateMember, Drawable {
        using StateMember::StateMember;
        void draw() const override;
    };
    struct ImPlotMetrics : StateMember, Drawable {
        using StateMember::StateMember;
        void draw() const override;
    };

    FlowGridMetrics flowgrid{path, "flowgrid", "FlowGrid"};
    ImGuiMetrics imgui{path, "imgui", "ImGui"};
    ImPlotMetrics implot{path, "implot", "ImPlot"};
};

struct Tools : Window {
    using Window::Window;
    void draw() const override;
};

struct Audio : Process {
    using Process::Process;

    enum Backend { none, dummy, alsa, pulseaudio, jack, coreaudio, wasapi };
    static const std::vector<int> PrioritizedDefaultSampleRates;

    struct Settings : Window {
        using Window::Window;
        void draw() const override;

        Backend backend = none;
        std::optional<string> in_device_id;
        std::optional<string> out_device_id;
        bool muted = true;
        float device_volume = 1.0;
        int sample_rate = PrioritizedDefaultSampleRates[0];
    };

    struct Faust : StateMember {
        using StateMember::StateMember;

        struct Editor : Window {
            using Window::Window;
            void draw() const override;

            string file_name{"default.dsp"};
        };

        // The following are populated by `StatefulFaustUI` when the Faust DSP changes.
        // TODO thinking basically move members out of `StatefulFaustUI` more or less as is into the main state here.
        struct Log : Window {
            using Window::Window;
            void draw() const override;
        };

        Editor editor{path, "editor", "Faust editor"};
        Log log{path, "log", "Faust log"};

        //    string code{"import(\"stdfaust.lib\");\n\n"
//                     "pitchshifter = vgroup(\"Pitch Shifter\", ef.transpose(\n"
//                     "    hslider(\"window (samples)\", 1000, 50, 10000, 1),\n"
//                     "    hslider(\"xfade (samples)\", 10, 1, 10000, 1),\n"
//                     "    hslider(\"shift (semitones) \", 0, -24, +24, 0.1)\n"
//                     "  )\n"
//                     ");\n"
//                     "\n"
//                     "process = no.noise : pitchshifter;\n"};
        string code{"import(\"stdfaust.lib\");\n\nprocess = ba.pulsen(1, 10000) : pm.djembe(60, 0.3, 0.4, 1) <: dm.freeverb_demo;"};
        string error{};
    };

    void update_process() const override;

    Settings settings{path, "settings", "Audio settings"};
    Faust faust{path, "faust"};
};


using MessagePackBytes = std::vector<std::uint8_t>;

struct File : StateMember {
    using StateMember::StateMember;

    struct DialogData {
        // Always open as a modal to avoid user activity outside the dialog.
        DialogData(string title = "Choose file", string filters = "", string file_path = ".", string default_file_name = "",
                   const bool save_mode = false, const int &max_num_selections = 1, ImGuiFileDialogFlags flags = ImGuiFileDialogFlags_None)
            : visible{false}, save_mode(save_mode), max_num_selections(max_num_selections), flags(flags | ImGuiFileDialogFlags_Modal),
              title(std::move(title)), filters(std::move(filters)), file_path(std::move(file_path)), default_file_name(std::move(default_file_name)) {};

        bool visible;
        bool save_mode; // The same file dialog instance is used for both saving & opening files.
        int max_num_selections;
        ImGuiFileDialogFlags flags;
        string title;
        string filters;
        string file_path;
        string default_file_name;
    };

    // TODO window?
    struct Dialog : DialogData, StateMember {
        Dialog(const JsonPath &path, const string &id) : DialogData(), StateMember(path, id, title) {}

        Dialog &operator=(const DialogData &other) {
            DialogData::operator=(other);
            visible = true;
            return *this;
        }

        void draw() const;
    };

    static string read(const fs::path &path);
    static bool write(const fs::path &path, const string &contents);
    static bool write(const fs::path &path, const MessagePackBytes &contents);

    Dialog dialog{path, "dialog"};
};

enum FlowGridCol_ {
    FlowGridCol_GestureIndicator, // 2nd series in ImPlot color map (same in all 3 styles for now)
    FlowGridCol_HighlightText, // ImGuiCol_PlotHistogramHovered
    FlowGridCol_COUNT
};

typedef int FlowGridCol; // -> enum FlowGridCol_

const DurationSec FlashDurationSecMin = 0.0, FlashDurationSecMax = 5.0;

struct FlowGridStyle : StateMember, Drawable {
    FlowGridStyle(const JsonPath &parent_path, const string &id, const string &name = "") : StateMember(parent_path, id, name) {}

    void draw() const override;

    ImVec4 Colors[FlowGridCol_COUNT];
    DurationSec FlashDurationSec{0.6};

    void StyleColorsDark() {
        Colors[FlowGridCol_HighlightText] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        Colors[FlowGridCol_GestureIndicator] = ImPlot::GetColormapColor(1, 0);
    }
    void StyleColorsClassic() {
        Colors[FlowGridCol_HighlightText] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        Colors[FlowGridCol_GestureIndicator] = ImPlot::GetColormapColor(1, 0);
    }
    void StyleColorsLight() {
        Colors[FlowGridCol_HighlightText] = ImVec4(1.00f, 0.45f, 0.00f, 1.00f);
        Colors[FlowGridCol_GestureIndicator] = ImPlot::GetColormapColor(1, 0);
    }

    static const char *GetColorName(FlowGridCol idx) {
        switch (idx) {
            case FlowGridCol_GestureIndicator: return "GestureIndicator";
            case FlowGridCol_HighlightText: return "HighlightText";
            default: return "Unknown";
        }
    }
};

struct Style : Window {
    using Window::Window;

    void draw() const override;

    struct ImGuiStyleMember : StateMember, Drawable, ImGuiStyle {
        using StateMember::StateMember;
        void draw() const override;
    };
    struct ImPlotStyleMember : StateMember, Drawable, ImPlotStyle {
        using StateMember::StateMember;
        void draw() const override;
    };

    ImGuiStyleMember imgui{path, "imgui", "ImGui"};
    ImPlotStyleMember implot{path, "implot", "ImPlot"};
    FlowGridStyle flowgrid{path, "flowgrid", "FlowGrid"};
};

struct Processes : StateMember {
    using StateMember::StateMember;

    Process ui{path, "ui", "UI"};
};

// The definition of `ImGuiDockNodeSettings` is not exposed (it's defined in `imgui.cpp`).
// This is a copy, and should be kept up-to-date with that definition.
struct ImGuiDockNodeSettings {
    ImGuiID ID{};
    ImGuiID ParentNodeId{};
    ImGuiID ParentWindowId{};
    ImGuiID SelectedTabId{};
    signed char SplitAxis{};
    char Depth{};
    ImGuiDockNodeFlags Flags{};
    ImVec2ih Pos{};
    ImVec2ih Size{};
    ImVec2ih SizeRef{};
};

struct ImGuiSettingsData {
    ImGuiSettingsData() = default;
    explicit ImGuiSettingsData(ImGuiContext *ctx);

    ImVector<ImGuiDockNodeSettings> nodes;
    ImVector<ImGuiWindowSettings> windows;
    ImVector<ImGuiTableSettings> tables;
};

struct ImGuiSettings : StateMember, ImGuiSettingsData {
    ImGuiSettings(const JsonPath &parent_path, const string &id, const string &name = "") : StateMember(parent_path, id, name), ImGuiSettingsData() {}

    ImGuiSettings &operator=(const ImGuiSettingsData &other) {
        ImGuiSettingsData::operator=(other);
        return *this;
    }

    // Inverse of above constructor. `imgui_context.settings = this`
    // Should behave just like `ImGui::LoadIniSettingsFromMemory`, but using the structured `...Settings` members
    // in this struct instead of the serialized .ini text format.
    // TODO table settings
    void populate_context(ImGuiContext *ctx) const;
};

const JsonPath RootPath{""};

struct StateData {
    ImGuiSettings imgui_settings{RootPath, "imgui_settings", "ImGui settings"};
    Style style{RootPath, "style"};
    ApplicationSettings application_settings{RootPath, "application_settings"};
    Audio audio{RootPath, "audio"};
    Processes processes{RootPath, "processes"};
    File file{RootPath, "file"};

    Demo demo{RootPath, "demo"};
    Metrics metrics{RootPath, "metrics"};
    Tools tools{RootPath, "tools"};

    StateViewer state_viewer{RootPath, "state_viewer"};
    StateMemoryEditor state_memory_editor{RootPath, "state_memory_editor"};
    StatePathUpdateFrequency path_update_frequency{RootPath, "path_update_frequency", "State path update frequency"};
    ProjectPreview project_preview{RootPath, "project_preview"};
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
    JsonPath path;
    JsonPatchOpType op{};
    std::optional<json> value; // Present for add/replace/test
    std::optional<string> from; // Present for copy/move
};
using JsonPatch = std::vector<JsonPatchOp>;

// One issue with this data structure is that forward & reverse diffs both redundantly store the same json path(s).
struct BidirectionalStateDiff {
    JsonPatch forward;
    JsonPatch reverse;
    TimePoint time;
};
using Diffs = std::vector<BidirectionalStateDiff>;

NLOHMANN_JSON_SERIALIZE_ENUM(JsonPatchOpType, {
    { Add, "add" },
    { Remove, "remove" },
    { Replace, "replace" },
    { Copy, "copy" },
    { Move, "move" },
    { Test, "test" },
})

JsonType(JsonPatchOp, path, op, value)
JsonType(BidirectionalStateDiff, forward, reverse, time)

JsonType(ImVec2, x, y)
JsonType(ImVec4, w, x, y, z)
JsonType(ImVec2ih, x, y)

JsonType(Window, visible)
JsonType(Process, running)

JsonType(ApplicationSettings, visible, GestureDurationSec)
JsonType(Audio::Faust::Editor, visible, file_name)
JsonType(Audio::Faust, code, error, editor, log)
JsonType(Audio::Settings, visible, muted, backend, sample_rate, device_volume)
JsonType(Audio, running, settings, faust)
JsonType(File::Dialog, visible, title, save_mode, filters, file_path, default_file_name, max_num_selections, flags) // todo without this, error "type must be string, but is object" on project load
JsonType(File::DialogData, visible, title, save_mode, filters, file_path, default_file_name, max_num_selections, flags)
JsonType(File, dialog)
JsonType(StateViewer, visible, label_mode, auto_select)
JsonType(ProjectPreview, visible, format)
JsonType(Metrics::FlowGridMetrics, show_relative_paths)
JsonType(Metrics, visible, flowgrid)

JsonType(ImGuiStyle, Alpha, DisabledAlpha, WindowPadding, WindowRounding, WindowBorderSize, WindowMinSize, WindowTitleAlign, WindowMenuButtonPosition, ChildRounding, ChildBorderSize, PopupRounding, PopupBorderSize,
    FramePadding, FrameRounding, FrameBorderSize, ItemSpacing, ItemInnerSpacing, CellPadding, TouchExtraPadding, IndentSpacing, ColumnsMinSpacing, ScrollbarSize, ScrollbarRounding, GrabMinSize, GrabRounding,
    LogSliderDeadzone, TabRounding, TabBorderSize, TabMinWidthForCloseButton, ColorButtonPosition, ButtonTextAlign, SelectableTextAlign, DisplayWindowPadding, DisplaySafeAreaPadding, MouseCursorScale, AntiAliasedLines,
    AntiAliasedLinesUseTex, AntiAliasedFill, CurveTessellationTol, CircleTessellationMaxError, Colors)
JsonType(ImPlotStyle, LineWeight, Marker, MarkerSize, MarkerWeight, FillAlpha, ErrorBarSize, ErrorBarWeight, DigitalBitHeight, DigitalBitGap, PlotBorderSize, MinorAlpha, MajorTickLen, MinorTickLen, MajorTickSize,
    MinorTickSize, MajorGridSize, MinorGridSize, PlotPadding, LabelPadding, LegendPadding, LegendInnerPadding, LegendSpacing, MousePosPadding, AnnotationPadding, FitPadding, PlotDefaultSize, PlotMinSize, Colors,
    Colormap, UseLocalTime, UseISO8601, Use24HourClock)
JsonType(FlowGridStyle, Colors, FlashDurationSec)
JsonType(Style, visible, imgui, implot, flowgrid)

// Double-check occasionally that the fields in these ImGui settings definitions still match their ImGui counterparts.
JsonType(ImGuiDockNodeSettings, ID, ParentNodeId, ParentWindowId, SelectedTabId, SplitAxis, Depth, Flags, Pos, Size, SizeRef)
JsonType(ImGuiWindowSettings, ID, Pos, Size, ViewportPos, ViewportId, DockId, ClassId, DockOrder, Collapsed)
JsonType(ImGuiTableSettings, ID, SaveFlags, RefScale, ColumnsCount, ColumnsCountMax)
JsonType(ImGuiSettingsData, nodes, windows, tables)

JsonType(Processes, ui)

JsonType(StateData, application_settings, audio, file, style, imgui_settings, processes, state_viewer, state_memory_editor, path_update_frequency, project_preview, demo, metrics, tools);
