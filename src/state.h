#pragma once

#include "nlohmann/json.hpp"
#include "imgui.h"
#include "implot.h"

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


struct Dimensions {
    ImVec2 position;
    ImVec2 size;
};

struct WindowData {
    std::string name;
    bool visible{true};
};

struct Window : WindowData {
    Window() = default;
    // Don't copy/assign references!
    Window(const Window &other) : WindowData(other) {}

    virtual void draw() = 0;
    virtual void destroy() {};
};

struct WindowsData {
    struct FaustWindows {
        WindowData editor{"Faust editor"};
        WindowData log{"Faust log"};
    };
    struct StateWindows {
        struct StateViewerWindow : public WindowData {
            struct Settings {
                enum LabelMode { annotated, raw };
                LabelMode label_mode{annotated};
            };

            Settings settings{};
        };

        StateViewerWindow viewer{{"State viewer"}};
        WindowData memory_editor{"State memory editor"};
        WindowData path_update_frequency{"Path update frequency"};
    };

    StateWindows state{};
    WindowData controls{"Controls"};
    WindowData style_editor{"Style editor"};
    WindowData demos{"Demos"};
    WindowData metrics{"Metrics"};
    FaustWindows faust{};
};

struct Windows : public WindowsData {
    Windows() = default;
    // Don't copy/assign references!
    explicit Windows(const WindowsData &other) : WindowsData(other) {}

    Windows &operator=(const Windows &other) {
        WindowsData::operator=(other);
        return *this;
    }

    WindowData &named(const std::string &name) {
        for (auto &window: all) {
            if (name == window.get().name) return window;
        }
        throw std::invalid_argument(name);
    }

    const WindowData &named(const std::string &name) const {
        for (auto &window: all_const) {
            if (name == window.get().name) return window;
        }
        throw std::invalid_argument(name);
    }

    std::vector<std::reference_wrapper<WindowData>> all{controls, state.viewer, state.memory_editor, state.path_update_frequency, style_editor, demos, metrics, faust.editor, faust.log};
    std::vector<std::reference_wrapper<const WindowData>> all_const{controls, state.viewer, state.memory_editor, state.path_update_frequency, style_editor, demos, metrics, faust.editor, faust.log};
};

struct UiState { // Avoid name-clash with faust's `UI` class
    struct Style {
        ImGuiStyle imgui;
        ImPlotStyle implot;
    };
    bool running = true;
    Windows windows;
    Style style;
};

enum AudioBackend {
    none, dummy, alsa, pulseaudio, jack, coreaudio, wasapi
};

struct Editor {
    std::string file_name;
};

struct Faust {
//    std::string code{"import(\"stdfaust.lib\");\n\n"
//                     "pitchshifter = vgroup(\"Pitch Shifter\", ef.transpose(\n"
//                     "    hslider(\"window (samples)\", 1000, 50, 10000, 1),\n"
//                     "    hslider(\"xfade (samples)\", 10, 1, 10000, 1),\n"
//                     "    hslider(\"shift (semitones) \", 0, -24, +24, 0.1)\n"
//                     "  )\n"
//                     ");\n"
//                     "\n"
//                     "process = no.noise : pitchshifter;\n"};
    std::string code{"import(\"stdfaust.lib\");\n\nprocess = ba.pulsen(1, 10000) : pm.djembe(60, 0.3, 0.4, 1) <: dm.freeverb_demo;"};
    std::string error{};
    Editor editor{"default.dsp"};

    // The following are populated by `StatefulFaustUI` when the Faust DSP changes.
    // TODO thinking basically move members out of `StatefulFaustUI` more or less as is into the main state here.

};

struct Audio {
    AudioBackend backend = none;
    Faust faust;
    char *in_device_id = nullptr;
    char *out_device_id = nullptr;
    bool running = true;
    bool muted = true;
    bool out_raw = false;
    int sample_rate = 48000;
    double latency = 0.0;

};

struct ActionConsumer {
    bool running = true;
};

struct State {
    UiState ui;
    Audio audio;
    ActionConsumer action_consumer;
};

// An exact copy of `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE`, but with a shorter name.
// Note: It's probably a good idea to occasionally check the definition in `nlohmann/json.cpp` for any changes.
#define JSON_TYPE(Type, ...)  \
    inline void to_json(nlohmann::json& nlohmann_json_j, const Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__)) } \
    inline void from_json(const nlohmann::json& nlohmann_json_j, Type& nlohmann_json_t) { NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, __VA_ARGS__)) }

JSON_TYPE(Editor, file_name)
JSON_TYPE(Faust, code, error, editor)
JSON_TYPE(Audio, running, muted, backend, latency, sample_rate, out_raw, faust)
JSON_TYPE(ImVec2, x, y)
JSON_TYPE(ImVec4, w, x, y, z)
JSON_TYPE(Dimensions, position, size)
JSON_TYPE(WindowData, name, visible)
JSON_TYPE(WindowsData::StateWindows::StateViewerWindow::Settings, label_mode)
JSON_TYPE(WindowsData::StateWindows, viewer, memory_editor, path_update_frequency)
JSON_TYPE(WindowsData::FaustWindows, editor, log)
JSON_TYPE(WindowsData, controls, state, style_editor, demos, metrics, faust)
JSON_TYPE(ImGuiStyle, Alpha, DisabledAlpha, WindowPadding, WindowRounding, WindowBorderSize, WindowMinSize, WindowTitleAlign, WindowMenuButtonPosition, ChildRounding, ChildBorderSize, PopupRounding, PopupBorderSize,
    FramePadding, FrameRounding, FrameBorderSize, ItemSpacing, ItemInnerSpacing, CellPadding, TouchExtraPadding, IndentSpacing, ColumnsMinSpacing, ScrollbarSize, ScrollbarRounding, GrabMinSize, GrabRounding,
    LogSliderDeadzone, TabRounding, TabBorderSize, TabMinWidthForCloseButton, ColorButtonPosition, ButtonTextAlign, SelectableTextAlign, DisplayWindowPadding, DisplaySafeAreaPadding, MouseCursorScale, AntiAliasedLines,
    AntiAliasedLinesUseTex, AntiAliasedFill, CurveTessellationTol, CircleTessellationMaxError, Colors)
JSON_TYPE(ImPlotStyle, LineWeight, Marker, MarkerSize, MarkerWeight, FillAlpha, ErrorBarSize, ErrorBarWeight, DigitalBitHeight, DigitalBitGap, PlotBorderSize, MinorAlpha, MajorTickLen, MinorTickLen, MajorTickSize,
    MinorTickSize, MajorGridSize, MinorGridSize, PlotPadding, LabelPadding, LegendPadding, LegendInnerPadding, LegendSpacing, MousePosPadding, AnnotationPadding, FitPadding, PlotDefaultSize, PlotMinSize, Colors,
    Colormap, AntiAliasedLines, UseLocalTime, UseISO8601, Use24HourClock)
JSON_TYPE(UiState::Style, imgui, implot)
JSON_TYPE(UiState, running, windows, style)
JSON_TYPE(ActionConsumer, running)
JSON_TYPE(State, ui, audio, action_consumer);
