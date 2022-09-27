#pragma once

#include <set>

#include "ImGuiFileDialog.h"
#include "UI/UIContext.h"

#include "JsonType.h"
#include "Helper/String.h"

using namespace fmt;

// E.g. '/foo/bar/baz' => 'baz'
inline string path_variable_name(const JsonPath &path) { return path.back(); }

inline string path_label(const JsonPath &path) { return snake_case_to_sentence_case(path_variable_name(path)); }

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
using fsec = std::chrono::duration<float>; // float seconds as a std::chrono::duration
using TimePoint = Clock::time_point;

JsonType(ImVec2, x, y)
JsonType(ImVec4, w, x, y, z)
JsonType(ImVec2ih, x, y)

struct StateMember {
    StateMember(const JsonPath &parent_path, const string &id, const string &name = "")
        : Path(parent_path / id), ID(id), Name(name.empty() ? snake_case_to_sentence_case(id) : name) {}

    JsonPath Path; // todo add start byte offset relative to state root, and link from state viewer json nodes to memory editor
    string ID, Name;
};

struct Drawable {
    virtual void draw() const = 0;
};

// A `Field` is a drawable state-member that wraps around a primitive type.
namespace Field {
struct Base : StateMember {
    using StateMember::StateMember;

    virtual bool Draw() const = 0;

    string help;

protected:
    // Helper to display a (?) mark which shows a tooltip when hovered. From `imgui_demo.cpp`.
    void HelpMarker(bool after = true) const;
};

struct Bool : Base {
    Bool(const JsonPath &parent_path, const string &id, const string &name = "", bool value = false, const string &help = "")
        : Base(parent_path, id, name), value(value) {
        this->help = help;
    }
    Bool(const JsonPath &parent_path, const string &id, bool value, const string &help = "")
        : Bool(parent_path, id, "", value, help) {}

    operator bool() const { return value; }
    Bool &operator=(bool v) {
        value = v;
        return *this;
    }

    bool Draw() const override;
    bool DrawMenu() const;

    bool value;
};
struct Int : Base {
    Int(const JsonPath &parent_path, const string &id, const string &name = "", int value = 0, int min = 0, int max = 100, const string &help = "")
        : Base(parent_path, id, name), value(value), min(min), max(max) {
        this->help = help;
    }
    Int(const JsonPath &parent_path, const string &id, int value, int min = 0, int max = 100, const string &help = "")
        : Int(parent_path, id, "", value, min, max, help) {}

    operator int() const { return value; }
    Int &operator=(int v) {
        value = v;
        return *this;
    }

    bool Draw() const override;
    bool Draw(const std::vector<int> &options) const;

    int value, min, max;
};

struct Float : Base {
    Float(const JsonPath &parent_path, const string &id, const string &name = "", float value = 0, float min = 0, float max = 1, const string &help = "")
        : Base(parent_path, id, name), value(value), min(min), max(max) {
        this->help = help;
    }
    Float(const JsonPath &parent_path, const string &id, float value, float min = 0, float max = 1, const string &help = "")
        : Float(parent_path, id, "", value, min, max, help) {}

    operator float() const { return value; }
    Float &operator=(float v) {
        value = v;
        return *this;
    }

    bool Draw() const override;
    bool Draw(const char *fmt, ImGuiSliderFlags flags = ImGuiSliderFlags_None) const;
    bool Draw(float v_speed, const char *fmt, ImGuiSliderFlags flags = ImGuiSliderFlags_None) const;

    float value, min, max;
};
struct Vec2 : Base {
    Vec2(const JsonPath &parent_path, const string &id, const string &name = "", ImVec2 value = {0, 0}, float min = 0, float max = 1, const string &help = "")
        : Base(parent_path, id, name), value(value), min(min), max(max) {
        this->help = help;
    }
    Vec2(const JsonPath &parent_path, const string &id, ImVec2 value, float min = 0, float max = 1, const string &help = "")
        : Vec2(parent_path, id, "", value, min, max, help) {}

    operator ImVec2() const { return value; }
    Vec2 &operator=(const ImVec2 &v) {
        value = v;
        return *this;
    }

    bool Draw() const override;
    bool Draw(const char *fmt, ImGuiSliderFlags flags = ImGuiSliderFlags_None) const;

    ImVec2 value;
    float min, max;
};
struct String : Base {
    String(const JsonPath &parent_path, const string &id, const string &name = "", string value = "")
        : Base(parent_path, id, name), value(std::move(value)) {}

    operator string() const { return value; }
    operator bool() const { return !value.empty(); }

    String &operator=(string v) {
        value = std::move(v);
        return *this;
    }
    bool operator==(const string &v) const { return value == v; }

    bool Draw() const override;
    bool Draw(const std::vector<string> &options) const;

    string value;
};
struct Enum : Base {
    Enum(const JsonPath &parent_path, const string &id, std::vector<string> options, int value = 0, const string &name = "", const string &help = "")
        : Base(parent_path, id, name), value(value), options(std::move(options)) {
        this->help = help;
    }
    Enum(const JsonPath &parent_path, const string &id, std::vector<string> options, const string &help = "")
        : Enum(parent_path, id, std::move(options), 0, "", help) {}

    operator int() const { return value; }

    bool Draw() const override;
    bool Draw(const std::vector<int> &options) const;
    bool DrawMenu() const;

    int value;
    std::vector<string> options;
};
}

using namespace Field;

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
}

struct Window : StateMember, Drawable {
    using StateMember::StateMember;
    Window(const JsonPath &parent_path, const string &id, const string &name = "", bool visible = true) : StateMember(parent_path, id, name) {
        this->Visible = visible;
    }

    Bool Visible{Path, "Visible", true};

    ImGuiWindow &FindImGuiWindow() const { return *ImGui::FindWindowByName(Name.c_str()); }
    void DrawWindow(ImGuiWindowFlags flags = ImGuiWindowFlags_None) const;
    void Dock(ImGuiID node_id) const;
    bool ToggleMenuItem() const;
    void SelectTab() const;
};

struct Process : Window {
    using Window::Window;

    void draw() const override;
    virtual void update_process() const {}; // Start/stop the thread based on the current `Running` state, and any other needed housekeeping.

    Bool Running{Path, "Running", true, format("Disabling completely ends the {} process.\nEnabling will start the process up again.", lowercase(Name))};
};

struct ApplicationSettings : Window {
    using Window::Window;
    void draw() const override;

    Float GestureDurationSec{Path, "GestureDurationSec", 0.5, 0, 5}; // Merge actions occurring in short succession into a single gesture
};

struct StateViewer : Window {
    using Window::Window;
    void draw() const override;

    enum LabelMode { Annotated, Raw };
    Enum LabelMode{
        Path, "LabelMode", {"Annotated", "Raw"},
        "The raw JSON state doesn't store keys for all items.\n"
        "For example, the main `ui.style.colors` state is a list.\n\n"
        "'Annotated' mode shows (highlighted) labels for such state items.\n"
        "'Raw' mode shows the state exactly as it is in the raw JSON state."
    };
    Bool AutoSelect{
        Path, "AutoSelect", "Auto-select", true,
        "When auto-select is enabled, state changes automatically open.\n"
        "The state viewer to the changed state node(s), closing all other state nodes.\n"
        "State menu items can only be opened or closed manually if auto-select is disabled."
    };
};

struct StateMemoryEditor : Window {
    using Window::Window;
    void draw() const override;
};

struct StatePathUpdateFrequency : Window {
    using Window::Window;
    void draw() const override;
};

enum ProjectFormat { None = 0, StateFormat, DiffFormat, ActionFormat };

struct ProjectPreview : Window {
    using Window::Window;
    void draw() const override;

    Enum Format{Path, "Format", {"None", "StateFormat", "DiffFormat", "ActionFormat"}, 1};
    Bool Raw{Path, "Raw"};
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
        Bool ShowRelativePaths{Path, "ShowRelativePaths", true};
    };
    struct ImGuiMetrics : StateMember, Drawable {
        using StateMember::StateMember;
        void draw() const override;
    };
    struct ImPlotMetrics : StateMember, Drawable {
        using StateMember::StateMember;
        void draw() const override;
    };

    FlowGridMetrics FlowGrid{Path, "FlowGrid"};
    ImGuiMetrics ImGui{Path, "ImGui"};
    ImPlotMetrics ImPlot{Path, "ImPlot"};
};

struct Tools : Window {
    using Window::Window;
    void draw() const override;
};

enum AudioBackend { none, dummy, alsa, pulseaudio, jack, coreaudio, wasapi };

// Starting at `-1` allows for using `IO` types as array indices.
enum IO_ {
    IO_None = -1,
    IO_In,
    IO_Out,
};
using IO = IO_;

constexpr IO IO_All[] = {IO_In, IO_Out};
constexpr int IO_Count = 2;

inline static string to_string(const IO io, const bool shorten = false) {
    switch (io) {
        case IO_In: return shorten ? "in" : "input";
        case IO_Out: return shorten ? "out" : "output";
        case IO_None: return "none";
    }
}

inline static std::ostream &operator<<(std::ostream &os, const IO &io) {
    os << to_string(io);
    return os;
}

struct Audio : Process {
    using Process::Process;

    // A selection of supported formats, corresponding to `SoundIoFormat`
    enum IoFormat_ {
        IoFormat_Invalid = 0,
        IoFormat_Float64NE,
        IoFormat_Float32NE,
        IoFormat_S32NE,
        IoFormat_S16NE,
    };
    using IoFormat = int;
    static const std::vector<IoFormat> PrioritizedDefaultFormats;
    static const std::vector<int> PrioritizedDefaultSampleRates;

    void draw() const override;

    struct FaustState : StateMember {
        using StateMember::StateMember;

        struct FaustEditor : Window {
            using Window::Window;
            void draw() const override;

            string FileName{"default.dsp"}; // todo state member & respond to changes, or remove from state
        };

        struct FaustDiagram : Window {
            using Window::Window;
            void draw() const override;

            struct DiagramSettings : StateMember {
                using StateMember::StateMember;

                Bool ScaleFill{Path, "ScaleFill", "Scale to window", false}; // This and `style.FlowGrid.DiagramScale` below are mutually exclusive (Setting this to `true` makes `DiagramScale` inactive.)
                Bool HoverShowRect{Path, "HoverShowRect", false};
                Bool HoverShowType{Path, "HoverShowType", false};
                Bool HoverShowChannels{Path, "HoverShowChannels", false};
                Bool HoverShowChildChannels{Path, "HoverShowChildChannels", false};
            };

            DiagramSettings Settings{Path, "Settings"};
        };

        // todo move to top-level Log
        struct FaustLog : Window {
            using Window::Window;
            void draw() const override;
        };

        FaustEditor Editor{Path, "Editor", "Faust editor"};
        FaustDiagram Diagram{Path, "Diagram", "Faust diagram"};
        FaustLog Log{Path, "Log", "Faust log"};

        String Code{Path, "Code", "Code", R"#(import("stdfaust.lib");
pitchshifter = vgroup("Pitch Shifter", ef.transpose(
    hslider("window (samples)", 1000, 50, 10000, 1),
    hslider("xfade (samples)", 10, 1, 10000, 1),
    hslider("shift (semitones)", 0, -24, +24, 0.1)
  )
);
process = _ : pitchshifter;)#"};
//        String Code{Path, "Code", "Code", R"(import("stdfaust.lib");
//process = ba.beat(240) : pm.djembe(60, 0.3, 0.4, 1) <: dm.freeverb_demo;)"};
//        String Code{Path, "Code", "Code", R"(import("stdfaust.lib");
//process = _:fi.highpass(2,1000):_;)"};
//        String Code{Path, "Code", "Code", R"(import("stdfaust.lib");
//ctFreq = hslider("cutoffFrequency",500,50,10000,0.01);
//q = hslider("q",5,1,30,0.1);
//gain = hslider("gain",1,0,1,0.01);
//process = no:noise : fi.resonlp(ctFreq,q,gain);)"};
        string Error{};
    };

    void update_process() const override;
    String get_device_id(IO io) const { return io == IO_In ? InDeviceId : OutDeviceId; }

    Bool FaustRunning{Path, "FaustRunning", true, "Disabling completely skips Faust computation when computing audio output."};
    Bool Muted{Path, "Muted", true, "Enabling sets all audio output to zero.\nAll audio computation will still be performed, so this setting does not affect CPU load."};
    AudioBackend Backend = none;
    String InDeviceId{Path, "InDeviceId", "In device ID"};
    String OutDeviceId{Path, "OutDeviceId", "Out device ID"};
    Int InSampleRate{Path, "InSampleRate"};
    Int OutSampleRate{Path, "OutSampleRate"};
    Enum InFormat{Path, "InFormat", {"Invalid", "Float64", "Float32", "Short32", "Short16"}, IoFormat_Invalid};
    Enum OutFormat{Path, "OutFormat", {"Invalid", "Float64", "Float32", "Short32", "Short16"}, IoFormat_Invalid};
    Float OutDeviceVolume{Path, "OutDeviceVolume", 1.0};
    Bool MonitorInput{Path, "MonitorInput", false, "Enabling adds the audio input stream directly to the audio output."};

    FaustState Faust{Path, "Faust"};
};

struct File : StateMember {
    using StateMember::StateMember;

    struct DialogData {
        // Always open as a modal to avoid user activity outside the dialog.
        DialogData(string title = "Choose file", string filters = "", string file_path = ".", string default_file_name = "",
                   const bool save_mode = false, const int &max_num_selections = 1, ImGuiFileDialogFlags flags = ImGuiFileDialogFlags_None)
            : Visible{false}, SaveMode(save_mode), MaxNumSelections(max_num_selections), Flags(flags | ImGuiFileDialogFlags_Modal),
              Title(std::move(title)), Filters(std::move(filters)), FilePath(std::move(file_path)), DefaultFileName(std::move(default_file_name)) {};

        bool Visible;
        bool SaveMode; // The same file dialog instance is used for both saving & opening files.
        int MaxNumSelections;
        ImGuiFileDialogFlags Flags;
        string Title;
        string Filters;
        string FilePath;
        string DefaultFileName;
    };

    // TODO window?
    struct FileDialog : DialogData, StateMember {
        FileDialog(const JsonPath &path, const string &id) : DialogData(), StateMember(path, id, Title) {}

        FileDialog &operator=(const DialogData &other) {
            DialogData::operator=(other);
            Visible = true;
            return *this;
        }

        void draw() const;
    };

    FileDialog Dialog{Path, "Dialog"};
};

enum FlowGridCol_ {
    FlowGridCol_GestureIndicator, // 2nd series in ImPlot color map (same in all 3 styles for now): `ImPlot::GetColormapColor(1, 0)`
    FlowGridCol_HighlightText, // ImGuiCol_PlotHistogramHovered
    // Faust diagram colors
    FlowGridCol_DiagramBg, // ImGuiCol_WindowBg
    FlowGridCol_DiagramText, // ImGuiCol_Text
    FlowGridCol_DiagramGroupTitle, // ImGuiCol_Text
    FlowGridCol_DiagramGroupStroke, // ImGuiCol_Border
    FlowGridCol_DiagramLine, // ImGuiCol_PlotLines
    FlowGridCol_DiagramLink, // ImGuiCol_Button
    FlowGridCol_DiagramInverter, // ImGuiCol_Text
    FlowGridCol_DiagramOrientationMark, // ImGuiCol_Text
    // The rest are box fill colors of various types. todo design these colors for Dark/Classic/Light profiles
    FlowGridCol_DiagramNormal,
    FlowGridCol_DiagramUi,
    FlowGridCol_DiagramSlot,
    FlowGridCol_DiagramNumber,

    FlowGridCol_COUNT
};
using FlowGridCol = int;

struct FlowGridStyle : StateMember, Drawable {
    FlowGridStyle(const JsonPath &parent_path, const string &id, const string &name = "") : StateMember(parent_path, id, name) {
        ColorsDark();
        DiagramColorsDark();
        DiagramLayoutFlowGrid();
    }

    void draw() const override;

    ImVec4 Colors[FlowGridCol_COUNT];
    Float FlashDurationSec{Path, "FlashDurationSec", 0.6, 0, 5};

    Int DiagramFoldComplexity{Path, "DiagramFoldComplexity", 3, 0, 20,
                              "Number of boxes within a diagram before folding into a sub-diagram. Setting to zero disables folding altogether, for a fully-expanded diagram."};
    Bool DiagramScaleLinked{Path, "DiagramScaleLinked", "Link X/Y", true}; // Link X/Y scale sliders, forcing them to the same value.
    Vec2 DiagramScale{Path, "DiagramScale", {1, 1}, 0.1, 10};
    Enum DiagramDirection{Path, "DiagramDirection", {"Left", "Right"}, ImGuiDir_Right};
    Bool DiagramRouteFrame{Path, "DiagramRouteFrame", false};
    Bool DiagramSequentialConnectionZigzag{Path, "DiagramSequentialConnectionZigzag", true}; // false allows for diagonal lines instead of zigzags instead of zigzags
    Bool DiagramOrientationMark{Path, "DiagramOrientationMark", true};
    Float DiagramOrientationMarkRadius{Path, "DiagramOrientationMarkRadius", 1.5, 0.5, 3};
    Float DiagramTopLevelMargin{Path, "DiagramTopLevelMargin", 20, 0, 40};
    Float DiagramDecorateMargin{Path, "DiagramDecorateMargin", 20, 0, 40};
    Float DiagramDecorateLineWidth{Path, "DiagramDecorateLineWidth", 1, 0, 4};
    Float DiagramDecorateCornerRadius{Path, "DiagramDecorateCornerRadius", 0, 0, 10};
    Float DiagramBoxCornerRadius{Path, "DiagramBoxCornerRadius", 0, 0, 10};
    Float DiagramBinaryHorizontalGapRatio{Path, "DiagramBinaryHorizontalGapRatio", 0.25, 0, 1};
    Float DiagramWireWidth{Path, "DiagramWireWidth", 1, 0.5, 4};
    Float DiagramWireGap{Path, "DiagramWireGap", 16, 10, 20};
    Vec2 DiagramGap{Path, "DiagramGap", {8, 8}, 0, 20};
    Vec2 DiagramArrowSize{Path, "DiagramArrowSize", {3, 2}, 1, 10};
    Float DiagramInverterRadius{Path, "DiagramInverterRadius", 3, 1, 5};

    void ColorsDark() {
        Colors[FlowGridCol_HighlightText] = {1.00f, 0.60f, 0.00f, 1.00f};
        Colors[FlowGridCol_GestureIndicator] = {0.87, 0.52, 0.32, 1};
    }
    void ColorsClassic() {
        Colors[FlowGridCol_HighlightText] = {1.00f, 0.60f, 0.00f, 1.00f};
        Colors[FlowGridCol_GestureIndicator] = {0.87, 0.52, 0.32, 1};
    }
    void ColorsLight() {
        Colors[FlowGridCol_HighlightText] = {1.00f, 0.45f, 0.00f, 1.00f};
        Colors[FlowGridCol_GestureIndicator] = {0.87, 0.52, 0.32, 1};
    }

    void DiagramColorsDark() {
        Colors[FlowGridCol_DiagramBg] = {0.06, 0.06, 0.06, 0.94};
        Colors[FlowGridCol_DiagramText] = {1, 1, 1, 1};
        Colors[FlowGridCol_DiagramGroupTitle] = {1, 1, 1, 1};
        Colors[FlowGridCol_DiagramGroupStroke] = {0.43, 0.43, 0.5, 0.5};
        Colors[FlowGridCol_DiagramLine] = {0.61, 0.61, 0.61, 1};
        Colors[FlowGridCol_DiagramLink] = {0.26, 0.59, 0.98, 0.4};
        Colors[FlowGridCol_DiagramInverter] = {1, 1, 1, 1};
        Colors[FlowGridCol_DiagramOrientationMark] = {1, 1, 1, 1};
        // Box fills
        Colors[FlowGridCol_DiagramNormal] = {0.29, 0.44, 0.63, 1};
        Colors[FlowGridCol_DiagramUi] = {0.28, 0.47, 0.51, 1};
        Colors[FlowGridCol_DiagramSlot] = {0.28, 0.58, 0.37, 1};
        Colors[FlowGridCol_DiagramNumber] = {0.96, 0.28, 0, 1};
    }
    void DiagramColorsClassic() {
        Colors[FlowGridCol_DiagramBg] = {0, 0, 0, 0.85};
        Colors[FlowGridCol_DiagramText] = {0.9, 0.9, 0.9, 1};
        Colors[FlowGridCol_DiagramGroupTitle] = {0.9, 0.9, 0.9, 1};
        Colors[FlowGridCol_DiagramGroupStroke] = {0.5, 0.5, 0.5, 0.5};
        Colors[FlowGridCol_DiagramLine] = {1, 1, 1, 1};
        Colors[FlowGridCol_DiagramLink] = {0.35, 0.4, 0.61, 0.62};
        Colors[FlowGridCol_DiagramInverter] = {0.9, 0.9, 0.9, 1};
        Colors[FlowGridCol_DiagramOrientationMark] = {0.9, 0.9, 0.9, 1};
        // Box fills
        Colors[FlowGridCol_DiagramNormal] = {0.29, 0.44, 0.63, 1};
        Colors[FlowGridCol_DiagramUi] = {0.28, 0.47, 0.51, 1};
        Colors[FlowGridCol_DiagramSlot] = {0.28, 0.58, 0.37, 1};
        Colors[FlowGridCol_DiagramNumber] = {0.96, 0.28, 0, 1};
    }
    void DiagramColorsLight() {
        Colors[FlowGridCol_DiagramBg] = {0.94, 0.94, 0.94, 1};
        Colors[FlowGridCol_DiagramText] = {0, 0, 0, 1};
        Colors[FlowGridCol_DiagramGroupTitle] = {0, 0, 0, 1};
        Colors[FlowGridCol_DiagramGroupStroke] = {0, 0, 0, 0.3};
        Colors[FlowGridCol_DiagramLine] = {0.39, 0.39, 0.39, 1};
        Colors[FlowGridCol_DiagramLink] = {0.26, 0.59, 0.98, 0.4};
        Colors[FlowGridCol_DiagramInverter] = {0, 0, 0, 1};
        Colors[FlowGridCol_DiagramOrientationMark] = {0, 0, 0, 1};
        // Box fills
        Colors[FlowGridCol_DiagramNormal] = {0.29, 0.44, 0.63, 1};
        Colors[FlowGridCol_DiagramUi] = {0.28, 0.47, 0.51, 1};
        Colors[FlowGridCol_DiagramSlot] = {0.28, 0.58, 0.37, 1};
        Colors[FlowGridCol_DiagramNumber] = {0.96, 0.28, 0, 1};
    }
    // Color Faust diagrams the same way Faust does when it renders to SVG.
    void DiagramColorsFaust() {
        Colors[FlowGridCol_DiagramBg] = {1, 1, 1, 1};
        Colors[FlowGridCol_DiagramText] = {1, 1, 1, 1};
        Colors[FlowGridCol_DiagramGroupTitle] = {0, 0, 0, 1};
        Colors[FlowGridCol_DiagramGroupStroke] = {0.2, 0.2, 0.2, 1};
        Colors[FlowGridCol_DiagramLine] = {0, 0, 0, 1};
        Colors[FlowGridCol_DiagramLink] = {0, 0.2, 0.4, 1};
        Colors[FlowGridCol_DiagramInverter] = {0, 0, 0, 1};
        Colors[FlowGridCol_DiagramOrientationMark] = {0, 0, 0, 1};
        // Box fills
        Colors[FlowGridCol_DiagramNormal] = {0.29, 0.44, 0.63, 1};
        Colors[FlowGridCol_DiagramUi] = {0.28, 0.47, 0.51, 1};
        Colors[FlowGridCol_DiagramSlot] = {0.28, 0.58, 0.37, 1};
        Colors[FlowGridCol_DiagramNumber] = {0.96, 0.28, 0, 1};
    }

    void DiagramLayoutFlowGrid() {
        DiagramSequentialConnectionZigzag = false;
        DiagramOrientationMark = false;
        DiagramTopLevelMargin = 10;
        DiagramDecorateMargin = 15;
        DiagramDecorateLineWidth = 2;
        DiagramDecorateCornerRadius = 5;
        DiagramBoxCornerRadius = 4;
        DiagramBinaryHorizontalGapRatio = 0.25;
        DiagramWireWidth = 1;
        DiagramWireGap = 16;
        DiagramGap = {8, 8};
        DiagramArrowSize = {3, 2};
        DiagramInverterRadius = 3;
    }
    // Lay out Faust diagrams the same way Faust does when it renders to SVG.
    void DiagramLayoutFaust() {
        DiagramSequentialConnectionZigzag = true;
        DiagramOrientationMark = true;
        DiagramTopLevelMargin = 20;
        DiagramDecorateMargin = 20;
        DiagramDecorateLineWidth = 1;
        DiagramBoxCornerRadius = 0;
        DiagramDecorateCornerRadius = 0;
        DiagramBinaryHorizontalGapRatio = 0.25;
        DiagramWireWidth = 1;
        DiagramWireGap = 16;
        DiagramGap = {8, 8};
        DiagramArrowSize = {3, 2};
        DiagramInverterRadius = 3;
    }

    static const char *GetColorName(FlowGridCol idx) {
        switch (idx) {
            case FlowGridCol_GestureIndicator: return "GestureIndicator";
            case FlowGridCol_HighlightText: return "HighlightText";
            case FlowGridCol_DiagramBg: return "DiagramBg";
            case FlowGridCol_DiagramGroupTitle: return "DiagramGroupTitle";
            case FlowGridCol_DiagramGroupStroke: return "DiagramGroupStroke";
            case FlowGridCol_DiagramLine: return "DiagramLine";
            case FlowGridCol_DiagramLink: return "DiagramLink";
            case FlowGridCol_DiagramNormal: return "DiagramNormal";
            case FlowGridCol_DiagramUi: return "DiagramUi";
            case FlowGridCol_DiagramSlot: return "DiagramSlot";
            case FlowGridCol_DiagramNumber: return "DiagramNumber";
            case FlowGridCol_DiagramInverter: return "DiagramInverter";
            case FlowGridCol_DiagramOrientationMark: return "DiagramOrientationMark";
            default: return "Unknown";
        }
    }
};

struct Style : Window {
    using Window::Window;

    void draw() const override;

    struct ImGuiStyleMember : StateMember, Drawable {
        ImGuiStyleMember(const JsonPath &parent_path, const string &id, const string &name = "") : StateMember(parent_path, id, name) {
            ImGui::StyleColorsDark(Colors);
        }

        void populate_context(ImGuiContext *ctx) const;
        void draw() const override;

        // See `ImGuiStyle` for field descriptions.
        // Initial values copied from `ImGuiStyle()` default constructor.
        // Ranges copied from `ImGui::StyleEditor`.
        // Double-check everything's up-to-date from time to time!
        Float Alpha{Path, "Alpha", 1, 0.2, 1}; // Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets).
        Float DisabledAlpha{Path, "DisabledAlpha", 0.6, 0, 1, "Additional alpha multiplier for disabled items (multiply over current value of Alpha)."};
        Vec2 WindowPadding{Path, "WindowPadding", ImVec2(8, 8), 0, 20};
        Float WindowRounding{Path, "WindowRounding", 0, 0, 12};
        Float WindowBorderSize{Path, "WindowBorderSize", 1};
        Vec2 WindowMinSize{Path, "WindowMinSize", ImVec2(32, 32)};
        Vec2 WindowTitleAlign{Path, "WindowTitleAlign", ImVec2(0, 0.5)};
        Enum WindowMenuButtonPosition{Path, "WindowMenuButtonPosition", {"Left", "Right"}, ImGuiDir_Left};
        Float ChildRounding{Path, "ChildRounding", 0, 0, 12};
        Float ChildBorderSize{Path, "ChildBorderSize", 1};
        Float PopupRounding{Path, "PopupRounding", 0, 0, 12};
        Float PopupBorderSize{Path, "PopupBorderSize", 1};
        Vec2 FramePadding{Path, "FramePadding", ImVec2(4, 3), 0, 20};
        Float FrameRounding{Path, "FrameRounding", 0, 0, 12};
        Float FrameBorderSize{Path, "FrameBorderSize", 0};
        Vec2 ItemSpacing{Path, "ItemSpacing", ImVec2(8, 4), 0, 20};
        Vec2 ItemInnerSpacing{Path, "ItemInnerSpacing", ImVec2(4, 4), 0, 20};
        Vec2 CellPadding{Path, "CellPadding", ImVec2(4, 2), 0, 20};
        Vec2 TouchExtraPadding{Path, "TouchExtraPadding", ImVec2(0, 0), 0, 10};
        Float IndentSpacing{Path, "IndentSpacing", 21, 0, 30};
        Float ColumnsMinSpacing{Path, "ColumnsMinSpacing", 6};
        Float ScrollbarSize{Path, "ScrollbarSize", 14, 1, 20};
        Float ScrollbarRounding{Path, "ScrollbarRounding", 9, 0, 12};
        Float GrabMinSize{Path, "GrabMinSize", 12, 1, 20};
        Float GrabRounding{Path, "GrabRounding", 0, 0, 12};
        Float LogSliderDeadzone{Path, "LogSliderDeadzone", 4, 0, 12};
        Float TabRounding{Path, "TabRounding", 4, 0, 12};
        Float TabBorderSize{Path, "TabBorderSize", 0};
        Float TabMinWidthForCloseButton{Path, "TabMinWidthForCloseButton", 0};
        Enum ColorButtonPosition{Path, "ColorButtonPosition", {"Left", "Right"}, ImGuiDir_Right};
        Vec2 ButtonTextAlign{Path, "ButtonTextAlign", ImVec2(0.5, 0.5), 0, 1, "Alignment applies when a button is larger than its text content."};
        Vec2 SelectableTextAlign{Path, "SelectableTextAlign", ImVec2(0, 0), 0, 1, "Alignment applies when a selectable is larger than its text content."};
        Vec2 DisplayWindowPadding{Path, "DisplayWindowPadding", ImVec2(19, 19)};
        Vec2 DisplaySafeAreaPadding{Path, "DisplaySafeAreaPadding", ImVec2(3, 3), 0, 30, "Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured)."};
        Float MouseCursorScale{Path, "MouseCursorScale", 1};
        Bool AntiAliasedLines{Path, "AntiAliasedLines", "Anti-aliased lines", true, "When disabling anti-aliasing lines, you'll probably want to disable borders in your style as well."};
        Bool AntiAliasedLinesUseTex{Path, "AntiAliasedLinesUseTex", "Anti-aliased lines use texture", true,
                                    "Faster lines using texture data. Require backend to render with bilinear filtering (not point/nearest filtering)."};
        Bool AntiAliasedFill{Path, "AntiAliasedFill", "Anti-aliased fill", true};
        Float CurveTessellationTol{Path, "CurveTessellationTol", "Curve tesselation tolerance", 1.25, 0.1, 10};
        Float CircleTessellationMaxError{Path, "CircleTessellationMaxError", 0.3, 0.1, 5};
        ImVec4 Colors[ImGuiCol_COUNT];
    };
    struct ImPlotStyleMember : StateMember, Drawable {
        ImPlotStyleMember(const JsonPath &parent_path, const string &id, const string &name = "") : StateMember(parent_path, id, name) {
            Colormap = ImPlotColormap_Deep;
            ImPlot::StyleColorsAuto(Colors);
        }

        void populate_context(ImPlotContext *ctx) const;
        void draw() const override;

        // See `ImPlotStyle` for field descriptions.
        // Initial values copied from `ImPlotStyle()` default constructor.
        // Ranges copied from `ImPlot::StyleEditor`.
        // Double-check everything's up-to-date from time to time!
        Float LineWeight{Path, "LineWeight", 1, 0, 5};
        Int Marker{Path, "Marker", ImPlotMarker_None};
        Float MarkerSize{Path, "MarkerSize", 4, 2, 10};
        Float MarkerWeight{Path, "MarkerWeight", 1, 0, 5};
        Float FillAlpha{Path, "FillAlpha", 1};
        Float ErrorBarSize{Path, "ErrorBarSize", 5, 0, 10};
        Float ErrorBarWeight{Path, "ErrorBarWeight", 1.5, 0, 5};
        Float DigitalBitHeight{Path, "DigitalBitHeight", 8, 0, 20};
        Float DigitalBitGap{Path, "DigitalBitGap", 4, 0, 20};
        Float PlotBorderSize{Path, "PlotBorderSize", 1, 0, 2};
        Float MinorAlpha{Path, "MinorAlpha", 0.25};
        Vec2 MajorTickLen{Path, "MajorTickLen", ImVec2(10, 10), 0, 20};
        Vec2 MinorTickLen{Path, "MinorTickLen", ImVec2(5, 5), 0, 20};
        Vec2 MajorTickSize{Path, "MajorTickSize", ImVec2(1, 1), 0, 2};
        Vec2 MinorTickSize{Path, "MinorTickSize", ImVec2(1, 1), 0, 2};
        Vec2 MajorGridSize{Path, "MajorGridSize", ImVec2(1, 1), 0, 2};
        Vec2 MinorGridSize{Path, "MinorGridSize", ImVec2(1, 1), 0, 2};
        Vec2 PlotPadding{Path, "PlotPadding", ImVec2(10, 10), 0, 20};
        Vec2 LabelPadding{Path, "LabelPadding", ImVec2(5, 5), 0, 20};
        Vec2 LegendPadding{Path, "LegendPadding", ImVec2(10, 10), 0, 20};
        Vec2 LegendInnerPadding{Path, "LegendInnerPadding", ImVec2(5, 5), 0, 10};
        Vec2 LegendSpacing{Path, "LegendSpacing", ImVec2(5, 0), 0, 5};
        Vec2 MousePosPadding{Path, "MousePosPadding", ImVec2(10, 10), 0, 20};
        Vec2 AnnotationPadding{Path, "AnnotationPadding", ImVec2(2, 2), 0, 5};
        Vec2 FitPadding{Path, "FitPadding", ImVec2(0, 0), 0, 0.2};
        Vec2 PlotDefaultSize{Path, "PlotDefaultSize", ImVec2(400, 300), 0, 1000};
        Vec2 PlotMinSize{Path, "PlotMinSize", ImVec2(200, 150), 0, 300};
        ImVec4 Colors[ImPlotCol_COUNT];
        ImPlotColormap Colormap;
        Bool UseLocalTime{Path, "UseLocalTime"};
        Bool UseISO8601{Path, "UseISO8601"};
        Bool Use24HourClock{Path, "Use24HourClock"};
    };

    ImGuiStyleMember ImGui{Path, "ImGui"};
    ImPlotStyleMember ImPlot{Path, "ImPlot"};
    FlowGridStyle FlowGrid{Path, "FlowGrid"};
};

struct Processes : StateMember {
    using StateMember::StateMember;

    Process UI{Path, "UI"};
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

    ImVector<ImGuiDockNodeSettings> Nodes;
    ImVector<ImGuiWindowSettings> Windows;
    ImVector<ImGuiTableSettings> Tables;
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
    ImGuiSettings ImGuiSettings{RootPath, "ImGuiSettings", "ImGui settings"};
    Style Style{RootPath, "Style"};
    ApplicationSettings ApplicationSettings{RootPath, "ApplicationSettings", "Application settings"};
    Audio Audio{RootPath, "Audio"};
    Processes Processes{RootPath, "Processes"};
    File File{RootPath, "File"};

    Demo Demo{RootPath, "Demo"};
    Metrics Metrics{RootPath, "Metrics"};
    Tools Tools{RootPath, "Tools"};

    StateViewer StateViewer{RootPath, "StateViewer", "State viewer"};
    StateMemoryEditor StateMemoryEditor{RootPath, "StateMemoryEditor", "State memory editor"};
    StatePathUpdateFrequency PathUpdateFrequency{RootPath, "PathUpdateFrequency", "State path update frequency"};
    ProjectPreview ProjectPreview{RootPath, "ProjectPreview", "Project preview"};
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
    JsonPatch Forward;
    JsonPatch Reverse;
    TimePoint Time;
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

JsonType(JsonPatchOp, path, op, value, from) // lower-case since these are deserialized and passed directly to json-lib.
JsonType(BidirectionalStateDiff, Forward, Reverse, Time)

JsonType(Window, Visible)
JsonType(Process, Running)

JsonType(ApplicationSettings, Visible, GestureDurationSec)
JsonType(Audio::FaustState::FaustEditor, Visible, FileName)
JsonType(Audio::FaustState::FaustDiagram::DiagramSettings, ScaleFill, HoverShowRect, HoverShowType, HoverShowChannels, HoverShowChildChannels)
JsonType(Audio::FaustState::FaustDiagram, Settings)
JsonType(Audio::FaustState, Code, Diagram, Error, Editor, Log)
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
JsonType(FlowGridStyle, Colors, FlashDurationSec, DiagramFoldComplexity, DiagramDirection, DiagramSequentialConnectionZigzag, DiagramOrientationMark, DiagramOrientationMarkRadius, DiagramRouteFrame, DiagramScaleLinked,
    DiagramScale, DiagramTopLevelMargin, DiagramDecorateMargin, DiagramDecorateLineWidth, DiagramDecorateCornerRadius, DiagramBoxCornerRadius, DiagramBinaryHorizontalGapRatio, DiagramWireGap, DiagramGap,
    DiagramWireWidth, DiagramArrowSize, DiagramInverterRadius)
JsonType(Style, Visible, ImGui, ImPlot, FlowGrid)

// Double-check occasionally that the fields in these ImGui settings definitions still match their ImGui counterparts.
JsonType(ImGuiDockNodeSettings, ID, ParentNodeId, ParentWindowId, SelectedTabId, SplitAxis, Depth, Flags, Pos, Size, SizeRef)
JsonType(ImGuiWindowSettings, ID, Pos, Size, ViewportPos, ViewportId, DockId, ClassId, DockOrder, Collapsed)
JsonType(ImGuiTableSettings, ID, SaveFlags, RefScale, ColumnsCount, ColumnsCountMax)
JsonType(ImGuiSettingsData, Nodes, Windows, Tables)

JsonType(Processes, UI)

JsonType(StateData, ApplicationSettings, Audio, File, Style, ImGuiSettings, Processes, StateViewer, StateMemoryEditor, PathUpdateFrequency, ProjectPreview, Demo, Metrics, Tools);
