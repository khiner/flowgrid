#pragma once

#include <string>
#include "imgui.h"
#include "imgui_internal.h"
#include "implot.h"

/**
 * `StateData` is a data-only struct which fully describes the application at any point in time.
 * All functions are defined in the `State` class.
 * Think of `StateData` as the declaration of the application state, and `State` as decorating `StateData` with functionality.
 * This structure allows for classes that only care about the shape of the data to just import `StateData`, without minimal dependencies
 * (only pulling in dependencies needed to represent the state data structures.
 * For example, `Action.h` only needs `StateData`, not `State`, since many actions use custom `StateData::...` data structures/enums as members.
 */

using std::string;

// Time declarations inspired by https://stackoverflow.com/a/14391562/780425
using namespace std::chrono_literals; // Support literals like `1s` or `500ms`
using Clock = std::chrono::system_clock; // Main system clock
using DurationSec = float; // floats used for main duration type
using fsec = std::chrono::duration<DurationSec>; // float seconds as a std::chrono::duration
using TimePoint = Clock::time_point;

struct WindowData {
    string name;
    bool visible{true};
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

enum FlowGridCol_ {
    FlowGridCol_Flash, // ImGuiCol_TitleBgActive
    FlowGridCol_HighlightText, // ImGuiCol_PlotHistogramHovered
    FlowGridCol_COUNT
};

typedef int FlowGridCol; // -> enum ImGuiCol_

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

struct StateData {
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

    struct Style : WindowData {
        Style() { name = "Style"; }

        ImGuiStyle imgui;
        ImPlotStyle implot;
        FlowGridStyle flowgrid;

    private:
        // `...StyleEditor` methods are drawn as tabs, and return `true` if style changes.
        static bool ImGuiStyleEditor();
        static bool ImPlotStyleEditor();
        static bool FlowGridStyleEditor();
    };

    struct Audio {
        struct Settings : WindowData {
            Settings() { name = "Audio settings"; }

            enum AudioBackend { none, dummy, alsa, pulseaudio, jack, coreaudio, wasapi };

            AudioBackend backend = none;
            char *in_device_id = nullptr;
            char *out_device_id = nullptr;
            bool muted = true;
            bool out_raw = false;
            int sample_rate = 48000;
            double latency = 0.0;
        };

        struct Faust {
            struct Editor : WindowData {
                Editor() : file_name{"default.dsp"} { name = "Faust editor"; }

                string file_name;
            };

            // The following are populated by `StatefulFaustUI` when the Faust DSP changes.
            // TODO thinking basically move members out of `StatefulFaustUI` more or less as is into the main state here.
            struct Log : WindowData {
                Log() { name = "Faust log"; }
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

    ImGuiSettings imgui_settings;
    Style style;
    Audio audio;
    Processes processes;
    File file;
    Windows windows{};
};
