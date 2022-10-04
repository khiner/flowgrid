#pragma once

#include "UI/UIContext.h"
#include "JsonType.h"
#include "Helper/String.h"
#include "Helper/Sample.h"

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

// Split the string on '?'.
// If there is no '?' in the provided string, the first element will have the full input string and the second element will be an empty string.
// todo don't split on escaped '\?'
static std::pair<string, string> parse_help_text(const string &str) {
    const auto help_split = str.find_first_of('?');
    const bool found = help_split != string::npos;
    return {found ? str.substr(0, help_split) : str, found ? str.substr(help_split + 1) : ""};
}

struct StateMember {
    StateMember(const JsonPath &parent_path, const string &id, const string &name_and_help = "")
        : Path(parent_path / id), ID(id) {
        const auto &[name, help] = parse_help_text(name_and_help);
        Name = name.empty() ? snake_case_to_sentence_case(id) : name;
        Help = help;
    }

    JsonPath Path; // todo add start byte offset relative to state root, and link from state viewer json nodes to memory editor
    string ID, Name, Help;

protected:
    // Helper to display a (?) mark which shows a tooltip when hovered.
    // Similar to the one in `imgui_demo.cpp`.
    void HelpMarker(bool after = true) const;
};

struct Drawable {
    virtual void draw() const = 0;
};

// A `Field` is a drawable state-member that wraps around a primitive type.
namespace Field {
struct Base : StateMember {
    using StateMember::StateMember;
    virtual bool Draw() const = 0;
};

struct Bool : Base {
    Bool(const JsonPath &parent_path, const string &id, const string &name = "", bool value = false) : Base(parent_path, id, name), value(value) {}
    Bool(const JsonPath &parent_path, const string &id, bool value) : Bool(parent_path, id, "", value) {}

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
    Int(const JsonPath &parent_path, const string &id, int value = 0, int min = 0, int max = 100, const string &name = "")
        : Base(parent_path, id, name), value(value), min(min), max(max) {}

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
    Float(const JsonPath &parent_path, const string &id, float value = 0, float min = 0, float max = 1, const string &name = "")
        : Base(parent_path, id, name), value(value), min(min), max(max) {}

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
    Vec2(const JsonPath &parent_path, const string &id, ImVec2 value = {0, 0}, float min = 0, float max = 1, const string &name = "")
        : Base(parent_path, id, name), value(value), min(min), max(max) {}

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
    Enum(const JsonPath &parent_path, const string &id, std::vector<string> names, int value = 0, const string &name = "")
        : Base(parent_path, id, name), value(value), names(std::move(names)) {}
    Enum(const JsonPath &parent_path, const string &id, std::vector<string> names)
        : Enum(parent_path, id, std::move(names), 0, "") {}

    operator int() const { return value; }

    bool Draw() const override;
    bool Draw(const std::vector<int> &options) const;
    bool DrawMenu() const;

    int value;
    std::vector<string> names;
};

// todo support mixed types - see `ImGui::CheckboxFlagsT`
// todo support nested categories
// todo make a `FlowGridParamsTableFlags` field.
// todo in state viewer, make `Annotated` label mode expand out each integer flag into a string list
struct Flags : Base {
    // All text after an optional '?' character for each name will be interpreted as an item help string.
    // E.g. `{"Foo?Does a thing", "Bar?Does a different thing", "Baz"}`
    Flags(const JsonPath &parent_path, const string &id, const std::vector<string> &names, int value = 0, const string &name = "")
        : Base(parent_path, id, name), value(value), names_and_help(names | transform(parse_help_text) | to<std::vector<std::pair<string, string>>>) {}
    Flags(const JsonPath &parent_path, const string &id, const std::vector<string> &names)
        : Flags(parent_path, id, names, 0, "") {}
    operator int() const { return value; }

    bool Draw() const override;
    bool DrawMenu() const;

    int value;
    std::vector<std::pair<string, string>> names_and_help;
};
} // End `Field` namespace

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

inline void to_json(json &j, const Flags &field) { j = field.value; }
inline void from_json(const json &j, Flags &field) { field.value = j; }
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

    Bool Running{Path, "Running", format("?Disabling completely ends the {} process.\nEnabling will start the process up again.", lowercase(Name)), true};
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
        Path, "LabelMode", {"Annotated", "Raw"}, Annotated,
        "?The raw JSON state doesn't store keys for all items.\n"
        "For example, the main `ui.style.colors` state is a list.\n\n"
        "'Annotated' mode shows (highlighted) labels for such state items.\n"
        "'Raw' mode shows the state exactly as it is in the raw JSON state."
    };
    Bool AutoSelect{
        Path, "AutoSelect",
        "Auto-select?When auto-select is enabled, state changes automatically open.\n"
        "The state viewer to the changed state node(s), closing all other state nodes.\n"
        "State menu items can only be opened or closed manually if auto-select is disabled.", true,
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

enum FaustDiagramHoverFlags_ {
    FaustDiagramHoverFlags_None = 0,
    FaustDiagramHoverFlags_ShowRect = 1 << 0,
    FaustDiagramHoverFlags_ShowType = 1 << 1,
    FaustDiagramHoverFlags_ShowChannels = 1 << 2,
    FaustDiagramHoverFlags_ShowChildChannels = 1 << 3,
};
using FaustDiagramHoverFlags = int;

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
                Flags HoverFlags{
                    Path, "HoverFlags",
                    {"ShowRect?Display the hovered node's bounding rectangle",
                     "ShowType?Display the hovered node's box type",
                     "ShowChannels?Display the hovered node's channel points and indices",
                     "ShowChildChannels?Display the channel points and indices for each of the hovered node's children"},
                    FaustDiagramHoverFlags_None,
                    "?Hovering over a node in the graph will display the selected information",
                };
            };

            DiagramSettings Settings{Path, "Settings"};
        };

        struct FaustParams : Window {
            using Window::Window;
            void draw() const override;
        };

        // todo move to top-level Log
        struct FaustLog : Window {
            using Window::Window;
            void draw() const override;
        };

        FaustEditor Editor{Path, "Editor", "Faust editor"};
        FaustDiagram Diagram{Path, "Diagram", "Faust diagram"};
        FaustParams Params{Path, "Params", "Faust params"};
        FaustLog Log{Path, "Log", "Faust log"};

//        String Code{Path, "Code", "Code", R"#(import("stdfaust.lib");
//pitchshifter = vgroup("Pitch Shifter", ef.transpose(
//    vslider("window (samples)", 1000, 50, 10000, 1),
//    vslider("xfade (samples)", 10, 1, 10000, 1),
//    vslider("shift (semitones)", 0, -24, +24, 0.1)
//  )
//);
//process = _ : pitchshifter;)#"};
//        String Code{Path, "Code", "Code", R"(import("stdfaust.lib");
//process = ba.beat(240) : pm.djembe(60, 0.3, 0.4, 1) <: dm.freeverb_demo;)"};
//        String Code{Path, "Code", "Code", R"(import("stdfaust.lib");
//process = _:fi.highpass(2,1000):_;)"};
//        String Code{Path, "Code", "Code", R"(import("stdfaust.lib");
//ctFreq = hslider("cutoffFrequency",500,50,10000,0.01);
//q = hslider("q",5,1,30,0.1);
//gain = hslider("gain",1,0,1,0.01);
//process = no:noise : fi.resonlp(ctFreq,q,gain);)"};

// Based on Faust::UITester.dsp
        String Code{Path, "Code", "Code", R"#(import("stdfaust.lib");
declare name "UI Tester";
declare version "1.0";
declare author "O. Guillerminet";
declare license "BSD";
declare copyright "(c) O. Guillerminet 2012";

vbox = vgroup("vbox",
    checkbox("check1"),
    checkbox("check2"),
    nentry("knob0[style:knob]", 60, 0, 127, 0.1)
);

sliders = hgroup("sliders",
    vslider("vslider1", 60, 0, 127, 0.1),
    vslider("vslider2", 60, 0, 127, 0.1),
    vslider("vslider3", 60, 0, 127, 0.1)
);

knobs = hgroup("knobs",
    vslider("knob1[style:knob]", 60, 0, 127, 0.1),
    vslider("knob2[style:knob]", 60, 0, 127, 0.1),
    vslider("knob3[style:knob]", 60, 0, 127, 0.1)
);

smallhbox1 = hgroup("small box 1",
    vslider("vslider5 [unit:Hz]", 60, 0, 127, 0.1),
    vslider("vslider6 [unit:Hz]", 60, 0, 127, 0.1),
    vslider("knob4[style:knob]", 60, 0, 127, 0.1),
    nentry("num1 [unit:f]", 60, 0, 127, 0.1),
    vbargraph("vbar1", 0, 127)
);

smallhbox2 = hgroup("small box 2",
    vslider("vslider7 [unit:Hz]", 60, 0, 127, 0.1),
    vslider("vslider8 [unit:Hz]", 60, 0, 127, 0.1),
    vslider("knob5[style:knob]", 60, 0, 127, 0.1),
    nentry("num2 [unit:f]", 60, 0, 127, 0.1),
    vbargraph("vbar2", 0, 127)
);

smallhbox3 = hgroup("small box 3",
    vslider("vslider9 [unit:Hz]", 60, 0, 127, 0.1),
    vslider("vslider10 [unit:m]", 60, 0, 127, 0.1),
    vslider("knob6[style:knob]", 60, 0, 127, 0.1),
    nentry("num3 [unit:f]", 60, 0, 127, 0.1),
    vbargraph("vbar3", 0, 127)
);

subhbox1 = hgroup("sub box 1",
    smallhbox2,
    smallhbox3
);

vmisc = vgroup("vmisc",
    vslider("vslider4 [unit:Hz]", 60, 0, 127, 0.1),
    button("button"),
    hslider("hslider [unit:Hz]", 60, 0, 127, 0.1),
    smallhbox1,
    subhbox1,
    hbargraph("hbar", 0, 127)
);

hmisc = hgroup("hmisc",
    vslider("vslider4 [unit:f]", 60, 0, 127, 0.1),
    button("button"),
    hslider("hslider", 60, 0, 127, 0.1),
    nentry("num [unit:f]", 60, 0, 127, 0.1),
    (63.5 : vbargraph("vbar", 0, 127)),
    (42.42 : hbargraph("hbar", 0, 127))
);

//------------------------- Process --------------------------------

process = tgroup("grp 1",
    vbox,
    sliders,
    knobs,
    vmisc,
    hmisc);)#"};
        string Error{};
    };

    void update_process() const override;
    String get_device_id(IO io) const { return io == IO_In ? InDeviceId : OutDeviceId; }

    Bool FaustRunning{Path, "FaustRunning", "?Disabling completely skips Faust computation when computing audio output.", true};
    Bool Muted{Path, "Muted", "?Enabling sets all audio output to zero.\nAll audio computation will still be performed, so this setting does not affect CPU load.", true};
    AudioBackend Backend = none;
    String InDeviceId{Path, "InDeviceId", "In device ID"};
    String OutDeviceId{Path, "OutDeviceId", "Out device ID"};
    Int InSampleRate{Path, "InSampleRate"};
    Int OutSampleRate{Path, "OutSampleRate"};
    Enum InFormat{Path, "InFormat", {"Invalid", "Float64", "Float32", "Short32", "Short16"}, IoFormat_Invalid};
    Enum OutFormat{Path, "OutFormat", {"Invalid", "Float64", "Float32", "Short32", "Short16"}, IoFormat_Invalid};
    Float OutDeviceVolume{Path, "OutDeviceVolume", 1.0};
    Bool MonitorInput{Path, "MonitorInput", "?Enabling adds the audio input stream directly to the audio output.", false};

    FaustState Faust{Path, "Faust"};
};

using ImGuiFileDialogFlags = int;
constexpr int FileDialogFlags_Modal = 1 << 27; // Copied from ImGuiFileDialog source with a different name to avoid redefinition. Brittle but we can avoid an include this way.

struct File : StateMember {
    using StateMember::StateMember;

    struct DialogData {
        // Always open as a modal to avoid user activity outside the dialog.
        DialogData(string title = "Choose file", string filters = "", string file_path = ".", string default_file_name = "",
                   const bool save_mode = false, const int &max_num_selections = 1, ImGuiFileDialogFlags flags = 0)
            : Visible{false}, SaveMode(save_mode), MaxNumSelections(max_num_selections), Flags(flags | FileDialogFlags_Modal),
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

enum HAlign_ {
    HAlign_Left,
    HAlign_Center,
    HAlign_Right,
};
enum VAlign_ {
    VAlign_Top,
    VAlign_Center,
    VAlign_Bottom,
};
using HAlign = int;
using VAlign = int;

struct ImVec2i {
    int x, y;
};
using Align = ImVec2i; // E.g. `{HAlign_Center, VAlign_Bottom}`

struct FlowGridStyle : StateMember, Drawable {
    FlowGridStyle(const JsonPath &parent_path, const string &id, const string &name = "") : StateMember(parent_path, id, name) {
        ColorsDark();
        DiagramColorsDark();
        DiagramLayoutFlowGrid();
    }

    void draw() const override;

    ImVec4 Colors[FlowGridCol_COUNT];
    Float FlashDurationSec{Path, "FlashDurationSec", 0.6, 0, 5};

    Int DiagramFoldComplexity{
        Path, "DiagramFoldComplexity", 3, 0, 20,
        "?Number of boxes within a diagram before folding into a sub-diagram.\n"
        "Setting to zero disables folding altogether, for a fully-expanded diagram."};
    Bool DiagramScaleLinked{Path, "DiagramScaleLinked", "?Link X/Y", true}; // Link X/Y scale sliders, forcing them to the same value.
    Bool DiagramScaleFill{
        Path, "DiagramScaleFill",
        "?Scale to fill the window.\n"
        "Enabling this setting deactivates other diagram scale settings.", false};
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

    Bool ParamsHeaderTitles{Path, "ParamsHeaderTitles", true};
    Enum ParamsAlignmentHorizontal{Path, "ParamsAlignmentHorizontal", {"Left", "Center", "Right"}, HAlign_Center};
    Enum ParamsAlignmentVertical{Path, "ParamsAlignmentVertical", {"Top", "Center", "Bottom"}, VAlign_Center};

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
        Float DisabledAlpha{Path, "DisabledAlpha", 0.6, 0, 1, "?Additional alpha multiplier for disabled items (multiply over current value of Alpha)."};
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
        Vec2 ButtonTextAlign{Path, "ButtonTextAlign", ImVec2(0.5, 0.5), 0, 1, "?Alignment applies when a button is larger than its text content."};
        Vec2 SelectableTextAlign{Path, "SelectableTextAlign", ImVec2(0, 0), 0, 1, "?Alignment applies when a selectable is larger than its text content."};
        Vec2 DisplayWindowPadding{Path, "DisplayWindowPadding", ImVec2(19, 19)};
        Vec2 DisplaySafeAreaPadding{Path, "DisplaySafeAreaPadding", ImVec2(3, 3), 0, 30, "?Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured)."};
        Float MouseCursorScale{Path, "MouseCursorScale", 1};
        Bool AntiAliasedLines{Path, "AntiAliasedLines", "Anti-aliased lines?When disabling anti-aliasing lines, you'll probably want to disable borders in your style as well.", true};
        Bool AntiAliasedLinesUseTex{Path, "AntiAliasedLinesUseTex", "Anti-aliased lines use texture?Faster lines using texture data. Require backend to render with bilinear filtering (not point/nearest filtering).",
                                    true};
        Bool AntiAliasedFill{Path, "AntiAliasedFill", "Anti-aliased fill", true};
        Float CurveTessellationTol{Path, "CurveTessellationTol", 1.25, 0.1, 10, "Curve tesselation tolerance"};
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
    ParamsHeaderTitles, ParamsAlignmentHorizontal, ParamsAlignmentVertical)
JsonType(Style, Visible, ImGui, ImPlot, FlowGrid)

// Double-check occasionally that the fields in these ImGui settings definitions still match their ImGui counterparts.
JsonType(ImGuiDockNodeSettings, ID, ParentNodeId, ParentWindowId, SelectedTabId, SplitAxis, Depth, Flags, Pos, Size, SizeRef)
JsonType(ImGuiWindowSettings, ID, Pos, Size, ViewportPos, ViewportId, DockId, ClassId, DockOrder, Collapsed)
JsonType(ImGuiTableSettings, ID, SaveFlags, RefScale, ColumnsCount, ColumnsCountMax)
JsonType(ImGuiSettingsData, Nodes, Windows, Tables)

JsonType(Processes, UI)

JsonType(StateData, ApplicationSettings, Audio, File, Style, ImGuiSettings, Processes, StateViewer, StateMemoryEditor, PathUpdateFrequency, ProjectPreview, Demo, Metrics, Tools);
