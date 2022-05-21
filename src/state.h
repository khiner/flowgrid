#pragma once

#include "nlohmann/json.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"
#include "implot_internal.h"

using std::string;

// TODO The time definitions/units here look nice https://stackoverflow.com/a/14391562, might want to adopt
using namespace std::chrono_literals;
using Clock = std::chrono::system_clock;
using Nanos = std::chrono::nanoseconds;
using Millis = std::chrono::milliseconds;
using SystemTimeDuration = Nanos;
using SystemTime = std::chrono::time_point<Clock, SystemTimeDuration>;
using SystemTimeMs = std::chrono::time_point<Clock, Millis>;

/**
 * `StateData` is a data-only struct which fully describes the application at any point in time.
 *
 * The entire codebase has read-only access to the immutable, single source-of-truth application state instance `s`.
 * The global `Context c` instance updates `s` when it receives an `Action a` instance via the global `BlockingConcurrentQueue<Action> q` action queue.
 *
 * After modifying its `c.s` instance (referred to globally as `s`), `c` updates its mutable `c.ui_s` copy (referred to globally as `ui_s`).
 * **State clients, such as the UI, can and do freely use this single, global, low-latency `ui_s` instance as the de-facto mutable copy of the source-of-truth `s`.**
 * For example, the UI passes (nested) `ui_s` members to ImGui widgets as direct value references.
 *
  * `{Stateful}` structs extend their data-only `{Stateful}Data` parents, adding derived (and always present) fields for commonly accessed,
 *   but expensive-to-compute derivations of their core (minimal but complete) data members.
 *  Many `{Stateful}` structs also implement convenience methods for complex state updates across multiple fields,
 *    or for generating less-frequently needed derived data.
 *
 * **The global `const StateData &s` and `State ui_s` instances are declared in `context.h` and instantiated in `main.cpp`.**
 */

// TODO should all state members implement the `std::path` interface, returning their path from the root `State`?
//  then we could do something like
//  ```cpp
//     const bool is_color = path.parent_path() == s.style.imgui.colors < ImGuiCol_COUNT;
//     const int color_index = std::stoi(path.);
//  ```

struct Drawable {
    virtual void draw() = 0;
};

struct WindowData {
    string name;
    bool visible{true};
};

struct Window : WindowData, Drawable {
    Window() = default;
};

struct Windows : Drawable {
    void draw() override;

    struct StyleEditor : Window {
        StyleEditor() { name = "Style editor"; }
        void draw() override;

    private:
        // `...StyleEditor` methods are drawn as tabs, and return `true` if style changes.
        static bool ImGuiStyleEditor();
        static bool ImPlotStyleEditor();
        static bool FlowGridStyleEditor();
    };

    struct Demo : Window {
        Demo() { name = "Demo"; }
        void draw() override;
    };

    struct Metrics : Window {
        Metrics() { name = "Metrics"; }
        void draw() override;
    };

    StyleEditor style_editor{};
    Demo demos{};
    Metrics metrics{};
};

enum AudioBackend {
    none, dummy, alsa, pulseaudio, jack, coreaudio, wasapi
};

struct Audio : Drawable {
    void draw() override;

    struct Settings : Window {
        Settings() { name = "Audio settings"; }
        void draw() override;

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
            Editor() : file_name{"default.dsp"} { name = "Faust editor"; }
            void draw() override;

            string file_name;
        };

        // The following are populated by `StatefulFaustUI` when the Faust DSP changes.
        // TODO thinking basically move members out of `StatefulFaustUI` more or less as is into the main state here.
        struct Log : Window {
            Log() { name = "Faust log"; }
            void draw() override;
        };

        Editor editor{};
        Log log{};

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

    Settings settings{};
    Faust faust{};
};

enum FlowGridCol_ {
    FlowGridCol_Flash, // ImGuiCol_TitleBgActive
    FlowGridCol_HighlightText, // ImGuiCol_PlotHistogramHovered
    FlowGridCol_COUNT
};

typedef int FlowGridCol; // -> enum ImGuiCol_

const ImU32 FlashDurationMsMin = 0, FlashDurationMsMax = Millis(5s).count();

struct FlowGridStyle {
    ImVec4 Colors[FlowGridCol_COUNT];
    ImU32 FlashDurationMs = 500;

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

struct Style {
    ImGuiStyle imgui;
    ImPlotStyle implot;
    FlowGridStyle flowgrid;

    Style() {
        // Transparent background. Need this to draw in background draw list, behind ImGui contents.
        // Specifically, using this for background rects in tree nodes. Can't find a better way...
        // TODO post an ImGui issue asking about a way to draw a rect _between_ the window bg color and the ImGui frame content
        imgui.Colors[ImGuiCol_WindowBg].w = 0;
    }
};

struct Processes {
    struct Process {
        bool running = true;
    };

    Process action_consumer;
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
    ImVector<ImGuiDockNodeSettings> nodes_settings;
    ImVector<ImGuiWindowSettings> windows_settings;
    ImVector<ImGuiTableSettings> tables_settings;

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
    Windows windows;
    Style style;
    Audio audio;
    Processes processes;

    struct StateWindows : Drawable {
        void draw() override;

        struct StateViewer : Window {
            StateViewer() { name = "State viewer"; }
            void draw() override;

            enum LabelMode { annotated, raw };
            LabelMode label_mode{annotated};
            bool auto_select{true};
        };

        struct StateMemoryEditor : Window {
            StateMemoryEditor() { name = "State memory editor"; }
            void draw() override;
        };

        struct StatePathUpdateFrequency : Window {
            StatePathUpdateFrequency() { name = "State path update frequency"; }
            void draw() override;
        };

        StateViewer viewer{};
        StateMemoryEditor memory_editor{};
        StatePathUpdateFrequency path_update_frequency{};
    };

    StateWindows state{};
};

struct State : StateData {
    State() = default;
    // Don't copy/assign references!
    explicit State(const StateData &other) : StateData(other) {}

    State &operator=(const State &other) {
        StateData::operator=(other);
        return *this;
    }

    std::vector<std::reference_wrapper<WindowData>> all_windows{
        state.viewer, state.memory_editor, state.path_update_frequency,
        windows.style_editor, windows.demos, windows.metrics,
        audio.settings, audio.faust.editor, audio.faust.log
    };
    std::vector<std::reference_wrapper<const WindowData>> all_windows_const{
        state.viewer, state.memory_editor, state.path_update_frequency,
        windows.style_editor, windows.demos, windows.metrics,
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

// An exact copy of `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE`, but with a shorter name.
// Note: It's probably a good idea to occasionally check the definition in `nlohmann/json.cpp` for any changes.
#define JSON_TYPE(Type, ...)  \
    inline void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    inline void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, __VA_ARGS__)) }

JSON_TYPE(ImVec2, x, y)
JSON_TYPE(ImVec4, w, x, y, z)
JSON_TYPE(ImVec2ih, x, y)

// Double-check occasionally that all fields in these ImGui settings definitions are present here!
// TODO don't think we need `WantApply`
//  (but may need to manually set it to true when setting in imgui, depends on the details there)
JSON_TYPE(ImGuiDockNodeSettings, ID, ParentNodeId, ParentWindowId, SelectedTabId, SplitAxis, Depth, Flags, Pos, Size, SizeRef)
JSON_TYPE(ImGuiWindowSettings, ID, Pos, Size, ViewportPos, ViewportId, DockId, ClassId, DockOrder, Collapsed, WantApply)
JSON_TYPE(ImGuiTableSettings, ID, SaveFlags, RefScale, ColumnsCount, ColumnsCountMax, WantApply)

JSON_TYPE(WindowData, name, visible)
JSON_TYPE(Windows, style_editor, demos, metrics)
JSON_TYPE(Audio::Faust::Editor, name, visible, file_name)
JSON_TYPE(Audio::Faust, code, error, editor, log)
JSON_TYPE(Audio::Settings, name, visible, muted, backend, latency, sample_rate, out_raw)
JSON_TYPE(Audio, settings, faust)

JSON_TYPE(ImGuiStyle, Alpha, DisabledAlpha, WindowPadding, WindowRounding, WindowBorderSize, WindowMinSize, WindowTitleAlign, WindowMenuButtonPosition, ChildRounding, ChildBorderSize, PopupRounding, PopupBorderSize,
    FramePadding, FrameRounding, FrameBorderSize, ItemSpacing, ItemInnerSpacing, CellPadding, TouchExtraPadding, IndentSpacing, ColumnsMinSpacing, ScrollbarSize, ScrollbarRounding, GrabMinSize, GrabRounding,
    LogSliderDeadzone, TabRounding, TabBorderSize, TabMinWidthForCloseButton, ColorButtonPosition, ButtonTextAlign, SelectableTextAlign, DisplayWindowPadding, DisplaySafeAreaPadding, MouseCursorScale, AntiAliasedLines,
    AntiAliasedLinesUseTex, AntiAliasedFill, CurveTessellationTol, CircleTessellationMaxError, Colors)
JSON_TYPE(ImPlotStyle, LineWeight, Marker, MarkerSize, MarkerWeight, FillAlpha, ErrorBarSize, ErrorBarWeight, DigitalBitHeight, DigitalBitGap, PlotBorderSize, MinorAlpha, MajorTickLen, MinorTickLen, MajorTickSize,
    MinorTickSize, MajorGridSize, MinorGridSize, PlotPadding, LabelPadding, LegendPadding, LegendInnerPadding, LegendSpacing, MousePosPadding, AnnotationPadding, FitPadding, PlotDefaultSize, PlotMinSize, Colors,
    Colormap, AntiAliasedLines, UseLocalTime, UseISO8601, Use24HourClock)
JSON_TYPE(FlowGridStyle, Colors, FlashDurationMs)
JSON_TYPE(Style, imgui, implot, flowgrid)

JSON_TYPE(Processes::Process, running)
JSON_TYPE(Processes, action_consumer, audio, ui)
JSON_TYPE(ImGuiSettings, nodes_settings, windows_settings, tables_settings)
JSON_TYPE(State::StateWindows::StateViewer, name, visible, label_mode, auto_select)
JSON_TYPE(State::StateWindows, viewer, memory_editor, path_update_frequency)
JSON_TYPE(StateData, audio, style, imgui_settings, windows, state, processes);
