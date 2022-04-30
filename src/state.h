#pragma once

#include "nlohmann/json.hpp"
#include "imgui.h"
#include "implot.h"

struct Dimensions {
    ImVec2 position;
    ImVec2 size;
};

struct Window {
    std::string name;
    bool visible{true};
};

// `WindowsBase` contains only data members.
// Derived fields and convenience methods are in `Windows`
struct WindowsBase {
    struct FaustWindows {
        Window editor{"Faust editor"};
        Window log{"Faust log"};
    };
    struct StateWindows {
        struct StateViewerWindow : public Window {
            struct Settings {
                enum LabelMode { annotated, raw };
                LabelMode label_mode{annotated};
            };

            Settings settings{};
        };

        StateViewerWindow viewer{{"State viewer"}};
        Window memory_editor{"State memory editor"};
        Window path_update_frequency{"Path update frequency"};
    };

    StateWindows state{};
    Window controls{"Controls"};
    Window style_editor{"Style editor"};
    Window demos{"Demos"};
    Window metrics{"Metrics"};
    FaustWindows faust{};
};

struct Windows : public WindowsBase {
    Windows() = default;
    // Don't copy/assign references!
    Windows(const Windows &other) : WindowsBase(other) {}
    Windows &operator=(const Windows &other) {
        WindowsBase::operator=(other);
        return *this;
    }

    Window &named(const std::string &name) {
        for (auto &window: all) {
            if (name == window.get().name) return window;
        }
        throw std::invalid_argument(name);
    }

    const Window &named(const std::string &name) const {
        for (auto &window: all_const) {
            if (name == window.get().name) return window;
        }
        throw std::invalid_argument(name);
    }

    std::vector<std::reference_wrapper<Window>> all{controls, state.viewer, state.memory_editor, state.path_update_frequency, style_editor, demos, metrics, faust.editor, faust.log};
    std::vector<std::reference_wrapper<const Window>> all_const{controls, state.viewer, state.memory_editor, state.path_update_frequency, style_editor, demos, metrics, faust.editor, faust.log};
};

struct UiState { // Avoid name-clash with faust's `UI` class
    bool running = true;
    Windows windows;
    ImGuiStyle style;
    ImPlotStyle implot_style;
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
JSON_TYPE(Window, name, visible)
JSON_TYPE(WindowsBase::StateWindows::StateViewerWindow::Settings, label_mode)
JSON_TYPE(WindowsBase::StateWindows, viewer, memory_editor, path_update_frequency)
JSON_TYPE(WindowsBase::FaustWindows, editor, log)
JSON_TYPE(WindowsBase, controls, state, style_editor, demos, metrics, faust)
JSON_TYPE(ImGuiStyle, Alpha, DisabledAlpha, WindowPadding, WindowRounding, WindowBorderSize, WindowMinSize, WindowTitleAlign, WindowMenuButtonPosition, ChildRounding, ChildBorderSize,
    PopupRounding, PopupBorderSize, FramePadding, FrameRounding, FrameBorderSize, ItemSpacing, ItemInnerSpacing, CellPadding, TouchExtraPadding, IndentSpacing, ColumnsMinSpacing, ScrollbarSize, ScrollbarRounding,
    GrabMinSize, GrabRounding, LogSliderDeadzone, TabRounding, TabBorderSize, TabMinWidthForCloseButton, ColorButtonPosition, ButtonTextAlign, SelectableTextAlign, DisplayWindowPadding, DisplaySafeAreaPadding,
    MouseCursorScale, AntiAliasedLines, AntiAliasedLinesUseTex, AntiAliasedFill, CurveTessellationTol, CircleTessellationMaxError, Colors)
JSON_TYPE(ImPlotStyle, LineWeight, Marker, MarkerSize, MarkerWeight, FillAlpha, ErrorBarSize, ErrorBarWeight, DigitalBitHeight, DigitalBitGap, PlotBorderSize, MinorAlpha, MajorTickLen,
    MinorTickLen, MajorTickSize, MinorTickSize, MajorGridSize, MinorGridSize, PlotPadding, LabelPadding, LegendPadding, LegendInnerPadding, LegendSpacing, MousePosPadding, AnnotationPadding, FitPadding, PlotDefaultSize,
    PlotMinSize, Colors, Colormap, AntiAliasedLines, UseLocalTime, UseISO8601, Use24HourClock)
JSON_TYPE(UiState, running, windows, style, implot_style)
JSON_TYPE(ActionConsumer, running)
JSON_TYPE(State, ui, audio, action_consumer);
