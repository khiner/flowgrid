#pragma once

#include <set>

#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "implot_internal.h"

#include "File.h"
#include "JsonType.h"

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

// Time declarations inspired by https://stackoverflow.com/a/14391562/780425
using namespace std::chrono_literals; // Support literals like `1s` or `500ms`
using Clock = std::chrono::system_clock; // Main system clock
using DurationSec = float; // floats used for main duration type
using fsec = std::chrono::duration<DurationSec>; // float seconds as a std::chrono::duration
using TimePoint = Clock::time_point;

struct Drawable {
    virtual void draw() const = 0;
};

struct WindowData {
    explicit WindowData(string name) : name(std::move(name)) {}
    string name;
    bool visible{true};
};

struct Window : WindowData, Drawable {
    explicit Window(const string &name) : WindowData(name) {}
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

struct Demo : Window {
    using Window::Window;
    void draw() const override;
};

struct Metrics : Window {
    using Window::Window;
    void draw() const override;

    bool show_relative_paths = true;
};

struct Tools : Window {
    using Window::Window;
    void draw() const override;
};

enum AudioBackend {
    none, dummy, alsa, pulseaudio, jack, coreaudio, wasapi
};

struct Audio {
    struct Settings : Window {
        using Window::Window;
        void draw() const override;

        AudioBackend backend = none;
        char *in_device_id = nullptr;
        char *out_device_id = nullptr;
        bool muted = true;
        bool out_raw = false;
        int sample_rate = 48000;
        double latency = 0.0;
    };

    struct Faust {
        struct Editor : Window {
            explicit Editor(const string &name) : Window(name), file_name{"default.dsp"} {}
            void draw() const override;

            string file_name;
        };

        // The following are populated by `StatefulFaustUI` when the Faust DSP changes.
        // TODO thinking basically move members out of `StatefulFaustUI` more or less as is into the main state here.
        struct Log : Window {
            using Window::Window;
            void draw() const override;
        };

        Editor editor{"Faust editor"};
        Log log{"Faust log"};

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

    Settings settings{"Audio settings"};
    Faust faust{};
};

enum FlowGridCol_ {
    FlowGridCol_Flash, // ImGuiCol_TitleBgActive
    FlowGridCol_HighlightText, // ImGuiCol_PlotHistogramHovered
    FlowGridCol_COUNT
};

typedef int FlowGridCol; // -> enum FlowGridCol_

const DurationSec FlashDurationSecMin = 0.0, FlashDurationSecMax = 5.0;

struct FlowGridStyle {
    ImVec4 Colors[FlowGridCol_COUNT];
    DurationSec FlashDurationSec{0.6};

    static void StyleColorsDark(FlowGridStyle &style) {
        auto *colors = style.Colors;
        colors[FlowGridCol_Flash] = ImVec4(0.16f, 0.29f, 0.48f, 1.00f);
        colors[FlowGridCol_HighlightText] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    }
    static void StyleColorsClassic(FlowGridStyle &style) {
        auto *colors = style.Colors;
        colors[FlowGridCol_Flash] = ImVec4(0.32f, 0.32f, 0.63f, 0.87f);
        colors[FlowGridCol_HighlightText] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    }
    static void StyleColorsLight(FlowGridStyle &style) {
        auto *colors = style.Colors;
        colors[FlowGridCol_Flash] = ImVec4(0.82f, 0.82f, 0.82f, 1.00f);
        colors[FlowGridCol_HighlightText] = ImVec4(1.00f, 0.45f, 0.00f, 1.00f);
    }

    FlowGridStyle() {
        StyleColorsDark(*this);
    }

    static const char *GetColorName(FlowGridCol idx) {
        switch (idx) {
            case FlowGridCol_Flash: return "Flash";
            case FlowGridCol_HighlightText: return "HighlightText";
            default: return "Unknown";
        }
    }
};

struct Style : Window {
    using Window::Window;

    void draw() const override;

    ImGuiStyle imgui;
    ImPlotStyle implot;
    FlowGridStyle flowgrid;

private:
    // `...StyleEditor` methods are drawn as tabs, and return `true` if style changes.
    static void ImGuiStyleEditor();
    static void ImPlotStyleEditor();
    static void FlowGridStyleEditor();
};

struct Processes {
    struct Process {
        bool running = true;
    };

    Process ui;
    Process audio;
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

struct ImGuiSettings {
    ImVector<ImGuiDockNodeSettings> nodes;
    ImVector<ImGuiWindowSettings> windows;
    ImVector<ImGuiTableSettings> tables;

    ImGuiSettings() = default;
    explicit ImGuiSettings(ImGuiContext *c);

    // Inverse of above constructor. `imgui_context.settings = this`
    // Should behave just like `ImGui::LoadIniSettingsFromMemory`, but using the structured `...Settings` members
    // in this struct instead of the serialized .ini text format.
    // TODO table settings
    void populate_context(ImGuiContext *c) const;
};

struct StateData {
    ImGuiSettings imgui_settings;
    Style style{"Style"};
    Audio audio;
    Processes processes;
    File file;

    Demo demo{"Demo"};
    Metrics metrics{"Metrics"};
    Tools tools{"Tools"};
    StateViewer state_viewer{"State viewer"};
    StateMemoryEditor memory_editor{"State memory editor"};
    StatePathUpdateFrequency path_update_frequency{"State path update frequency"};
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

JsonType(WindowData, visible)

JsonType(Audio::Faust::Editor, visible, file_name)
JsonType(Audio::Faust, code, error, editor, log)
JsonType(Audio::Settings, visible, muted, backend, latency, sample_rate, out_raw)
JsonType(Audio, settings, faust)
JsonType(File::Dialog, visible, title, save_mode, filters, path, default_file_name, max_num_selections, flags)
JsonType(File, dialog)
JsonType(StateViewer, visible, label_mode, auto_select)
JsonType(Metrics, visible, show_relative_paths)

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
JsonType(ImGuiSettings, nodes, windows, tables)

JsonType(Processes::Process, running)
JsonType(Processes, audio, ui)

JsonType(StateData, audio, file, style, imgui_settings, processes, state_viewer, memory_editor, path_update_frequency, demo, metrics, tools);
