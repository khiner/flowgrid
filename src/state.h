#pragma once

#include "nlohmann/json.hpp"
#include "imgui.h"

// TODO Different modes, with different states (e.g. AudioTrackMode),
//  which control the default settings for
//    * Layout
//    * Node organization, move-rules
//    * Automatic connections-rules

struct Dimensions {
    ImVec2 position;
    ImVec2 size;
};

struct Window {
    std::string name;
    bool visible{true};
};

struct Windows {
    struct ImGuiWindow : public Window {
        Window demo{"Dear ImGui Demo"};
        Window metrics{"Dear ImGui Metrics/Debugger"};
        Window style_editor{"Dear ImGui Style editor"};
    };
    struct FaustWindows {
        Window editor{"Faust"};
    };

    Window controls{"Controls"};
    ImGuiWindow imgui{{"ImGui", true}};
    FaustWindows faust;
};

struct UI {
    bool running = true;
    Windows windows;
    std::map<std::string, Window> window_named{
        {windows.controls.name,           windows.controls},
        {windows.imgui.demo.name,         windows.imgui.demo},
        {windows.imgui.metrics.name,      windows.imgui.metrics},
        {windows.imgui.style_editor.name, windows.imgui.style_editor},
        {windows.faust.editor.name,       windows.faust.editor},
    };
    ImGuiStyle style;
};

enum AudioBackend {
    none, dummy, alsa, pulseaudio, jack, coreaudio, wasapi
};

struct Faust {
    bool simple_text_editor;
    std::string code{"import(\"stdfaust.lib\"); process = no.noise;\n"};
    std::string error{};
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
    UI ui;
    Audio audio;
    ActionConsumer action_consumer;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Faust, simple_text_editor, code, error)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Audio, running, muted, backend, latency, sample_rate, out_raw, faust)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ImVec2, x, y)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ImVec4, w, x, y, z)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Dimensions, position, size)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Window, name, visible)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Windows::ImGuiWindow, name, visible, demo, metrics, style_editor)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Windows::FaustWindows, editor)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Windows, controls, imgui, faust)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ImGuiStyle, Alpha, DisabledAlpha, WindowPadding, WindowRounding, WindowBorderSize, WindowMinSize, WindowTitleAlign, WindowMenuButtonPosition, ChildRounding, ChildBorderSize,
    PopupRounding, PopupBorderSize, FramePadding, FrameRounding, FrameBorderSize, ItemSpacing, ItemInnerSpacing, CellPadding, TouchExtraPadding, IndentSpacing, ColumnsMinSpacing, ScrollbarSize, ScrollbarRounding,
    GrabMinSize, GrabRounding, LogSliderDeadzone, TabRounding, TabBorderSize, TabMinWidthForCloseButton, ColorButtonPosition, ButtonTextAlign, SelectableTextAlign, DisplayWindowPadding, DisplaySafeAreaPadding,
    MouseCursorScale, AntiAliasedLines, AntiAliasedLinesUseTex, AntiAliasedFill, CurveTessellationTol, CircleTessellationMaxError)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UI, running, windows, style, window_named)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ActionConsumer, running)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(State, ui, audio, action_consumer);
