#pragma once

/**
 * The main `State` instance fully describes the application at any point in time.
 *
 * The entire codebase has read-only access to the immutable, single source-of-truth application `const State &s` instance,
 * which also provides `Draw()` and `Update(const Action &)` methods.
 * This `s` instance is declared here, instantiated in the `Context` constructor, and assigned in `main.cpp`.
 */

#include <iostream>
#include <list>
#include <queue>
#include <set>

#include "nlohmann/json.hpp"
#include "fmt/chrono.h"

#include "UI/UIContext.h"
#include "Helper/String.h"
#include "Helper/Sample.h"
#include "Helper/File.h"
#include "Helper/UI.h"

namespace FlowGrid {}
namespace fg = FlowGrid;

using namespace fmt;
using namespace nlohmann;

// Time declarations inspired by https://stackoverflow.com/a/14391562/780425
using namespace std::chrono_literals; // Support literals like `1s` or `500ms`
using Clock = std::chrono::system_clock; // Main system clock
using fsec = std::chrono::duration<float>; // float seconds as a std::chrono::duration
using TimePoint = Clock::time_point;

using std::nullopt;
using JsonPath = json::json_pointer;
using std::cout, std::cerr;
using std::unique_ptr, std::make_unique;
using std::min, std::max;

// E.g. '/foo/bar/baz' => 'baz'
inline string path_variable_name(const JsonPath &path) { return path.back(); }
inline string path_label(const JsonPath &path) { return snake_case_to_sentence_case(path_variable_name(path)); }

// Split the string on '?'.
// If there is no '?' in the provided string, the first element will have the full input string and the second element will be an empty string.
// todo don't split on escaped '\?'
static std::pair<string, string> parse_help_text(const string &str) {
    const auto help_split = str.find_first_of('?');
    const bool found = help_split != string::npos;
    return {found ? str.substr(0, help_split) : str, found ? str.substr(help_split + 1) : ""};
}

struct Preferences {
    std::list<fs::path> recently_opened_paths;
};

static const JsonPath RootPath{""};

struct StateMember {
    static std::map<ImGuiID, StateMember *> WithID; // Allows for access of any state member by ImGui ID

    StateMember(const StateMember *parent = nullptr, const string &path_segment = "", const string &name_and_help = "")
        : Parent(parent),
          Path(Parent && !path_segment.empty() ? Parent->Path / path_segment : Parent ? Parent->Path : !path_segment.empty() ? JsonPath(path_segment) : RootPath),
          PathSegment(path_segment) {
        const auto &[name, help] = parse_help_text(name_and_help);
        Name = name.empty() ? path_segment.empty() ? "" : snake_case_to_sentence_case(path_segment) : name;
        Help = help;
        ID = ImHashStr(Name.c_str(), 0, Parent ? Parent->ID : 0);
        // This is real ugly - JSON assignment creates a temporary instance to copy from.
        // We don't want the destructor of that temporary instance to erase this ID.
        // Without this check, `WithID` will always end up empty!
        // See https://github.com/nlohmann/json/issues/1050
        if (WithID.contains(ID)) is_temp_instance = true;
        else WithID[ID] = this;
    }
    ~StateMember() {
        if (!is_temp_instance) WithID.erase(ID);
    }

    const StateMember *Parent;
    JsonPath Path;
    string PathSegment, Name, Help;
    ImGuiID ID;
    bool is_temp_instance{false};
    // todo add start byte offset relative to state root, and link from state viewer json nodes to memory editor

protected:
    // Helper to display a (?) mark which shows a tooltip when hovered.
    // Similar to the one in `imgui_demo.cpp`.
    void HelpMarker(bool after = true) const;
};

struct UIStateMember : StateMember {
    using StateMember::StateMember;
    virtual void Draw() const = 0;
};

// A `Field` is a drawable state-member that wraps around a primitive type.
namespace Field {
struct Base : StateMember {
    using StateMember::StateMember;
    virtual bool Draw() const = 0;
};

struct Bool : Base {
    Bool(const StateMember *parent, const string &path_segment, bool value = false, const string &name = "")
        : Base(parent, path_segment, name), value(value) {}

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
    Int(const StateMember *parent, const string &path_segment, int value = 0, int min = 0, int max = 100, const string &name = "")
        : Base(parent, path_segment, name), value(value), min(min), max(max) {}

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
    Float(const StateMember *parent, const string &path_segment, float value = 0, float min = 0, float max = 1, const string &name = "")
        : Base(parent, path_segment, name), value(value), min(min), max(max) {}

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
    Vec2(const StateMember *parent, const string &path_segment, ImVec2 value = {0, 0}, float min = 0, float max = 1, const string &name = "")
        : Base(parent, path_segment, name), value(value), min(min), max(max) {}

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
    String(const StateMember *parent, const string &path_segment, const string &name = "", string value = "")
        : Base(parent, path_segment, name), value(std::move(value)) {}

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
    Enum(const StateMember *parent, const string &path_segment, std::vector<string> names, int value = 0, const string &name = "")
        : Base(parent, path_segment, name), value(value), names(std::move(names)) {}

    operator int() const { return value; }

    bool Draw() const override;
    bool Draw(const std::vector<int> &options) const;
    bool DrawMenu() const;

    int value;
    std::vector<string> names;
};

// todo in state viewer, make `Annotated` label mode expand out each integer flag into a string list
struct Flags : Base {
    struct Item {
        Item(const char *name_and_help) {
            const auto &[name, help] = parse_help_text(name_and_help);
            Name = name;
            Help = help;
        }

        string Name, Help;
    };

    // All text after an optional '?' character for each name will be interpreted as an item help string.
    // E.g. `{"Foo?Does a thing", "Bar?Does a different thing", "Baz"}`
    Flags(const StateMember *parent, const string &path_segment, std::vector<Item> items, int value = 0, const string &name = "")
        : Base(parent, path_segment, name), value(value), items(std::move(items)) {}

    operator int() const { return value; }

    bool Draw() const override;
    bool DrawMenu() const;

    int value;
    std::vector<Item> items;
};
} // End `Field` namespace

using namespace Field;

// Subset of `ImGuiTableFlags`.
enum TableFlags_ {
    // Features
    TableFlags_Resizable = 1 << 0,
    TableFlags_Reorderable = 1 << 1,
    TableFlags_Hideable = 1 << 2,
    TableFlags_Sortable = 1 << 3,
    TableFlags_ContextMenuInBody = 1 << 4,
    // Borders
    TableFlags_BordersInnerH = 1 << 5,
    TableFlags_BordersOuterH = 1 << 6,
    TableFlags_BordersInnerV = 1 << 7,
    TableFlags_BordersOuterV = 1 << 8,
    TableFlags_Borders = TableFlags_BordersInnerH | TableFlags_BordersOuterH | TableFlags_BordersInnerV | TableFlags_BordersOuterV,
    TableFlags_NoBordersInBody = 1 << 9,
    // Padding
    TableFlags_PadOuterX = 1 << 10,
    TableFlags_NoPadOuterX = 1 << 11,
    TableFlags_NoPadInnerX = 1 << 12,
};
// todo 'Condensed' preset, with NoHostExtendX, NoBordersInBody, NoPadOuterX
using TableFlags = int;

enum ParamsWidthSizingPolicy_ {
    ParamsWidthSizingPolicy_StretchToFill, // If a table contains only fixed-width items, allow columns to stretch to fill available width.
    ParamsWidthSizingPolicy_StretchFlexibleOnly, // If a table contains only fixed-width items, it won't stretch to fill available width.
    ParamsWidthSizingPolicy_Balanced, // All param types are given flexible-width, weighted by their minimum width. (Looks more balanced, but less expansion room for wide items).
};
using ParamsWidthSizingPolicy = int;

static const std::vector<Flags::Item> TableFlagItems{
    "Resizable?Enable resizing columns",
    "Reorderable?Enable reordering columns in header row",
    "Hideable?Enable hiding/disabling columns in context menu",
    "Sortable?Enable sorting",
    "ContextMenuInBody?Right-click on columns body/contents will display table context menu. By default it is available in headers row.",
    "BordersInnerH?Draw horizontal borders between rows",
    "BordersOuterH?Draw horizontal borders at the top and bottom",
    "BordersInnerV?Draw vertical borders between columns",
    "BordersOuterV?Draw vertical borders on the left and right sides",
    "NoBordersInBody?Disable vertical borders in columns Body (borders will always appear in Headers)",
    "PadOuterX?Default if 'BordersOuterV' is on. Enable outermost padding. Generally desirable if you have headers.",
    "NoPadOuterX?Default if 'BordersOuterV' is off. Disable outermost padding.",
    "NoPadInnerX?Disable inner padding between columns (double inner padding if 'BordersOuterV' is on, single inner padding if 'BordersOuterV' is off)",
};

ImGuiTableFlags TableFlagsToImgui(TableFlags flags);

struct Window : UIStateMember {
    Window(const StateMember *parent, const string &path_segment, const string &name = "", const bool visible = true)
        : UIStateMember(parent, path_segment, name) {
        this->Visible = visible;
    }

    Bool Visible{this, "Visible", true};

    ImGuiWindow &FindImGuiWindow() const { return *ImGui::FindWindowByName(Name.c_str()); }
    void DrawWindow(ImGuiWindowFlags flags = ImGuiWindowFlags_None) const;
    void Dock(ImGuiID node_id) const;
    bool ToggleMenuItem() const;
    void SelectTab() const;
};

struct Process : Window {
    using Window::Window;

    void Draw() const override;
    virtual void update_process() const {}; // Start/stop the thread based on the current `Running` state, and any other needed housekeeping.

    Bool Running{this, "Running", true, format("?Disabling completely ends the {} process.\nEnabling will start the process up again.", lowercase(Name))};
};

struct ApplicationSettings : Window {
    using Window::Window;
    void Draw() const override;

    Float GestureDurationSec{this, "GestureDurationSec", 0.5, 0, 5}; // Merge actions occurring in short succession into a single gesture
};

struct StateViewer : Window {
    using Window::Window;
    void Draw() const override;

    enum LabelMode { Annotated, Raw };
    Enum LabelMode{
        this, "LabelMode", {"Annotated", "Raw"}, Annotated,
        "?The raw JSON state doesn't store keys for all items.\n"
        "For example, the main `ui.style.colors` state is a list.\n\n"
        "'Annotated' mode shows (highlighted) labels for such state items.\n"
        "'Raw' mode shows the state exactly as it is in the raw JSON state."
    };
    Bool AutoSelect{
        this, "AutoSelect", true,
        "Auto-select?When auto-select is enabled, state changes automatically open.\n"
        "The state viewer to the changed state node(s), closing all other state nodes.\n"
        "State menu items can only be opened or closed manually if auto-select is disabled."
    };
};

struct StateMemoryEditor : Window {
    using Window::Window;
    void Draw() const override;
};

struct StatePathUpdateFrequency : Window {
    using Window::Window;
    void Draw() const override;
};

enum ProjectFormat { None = 0, StateFormat, DiffFormat, ActionFormat };

struct ProjectPreview : Window {
    using Window::Window;
    void Draw() const override;

    Enum Format{this, "Format", {"None", "StateFormat", "DiffFormat", "ActionFormat"}, 1};
    Bool Raw{this, "Raw"};
};

struct Demo : Window {
    using Window::Window;
    void Draw() const override;

    struct ImGuiDemo : UIStateMember {
        using UIStateMember::UIStateMember;
        void Draw() const override;
    };
    struct ImPlotDemo : UIStateMember {
        using UIStateMember::UIStateMember;
        void Draw() const override;
    };
    struct FileDialogDemo : UIStateMember {
        using UIStateMember::UIStateMember;
        void Draw() const override;
    };

    ImGuiDemo ImGui{this, "ImGui"};
    ImPlotDemo ImPlot{this, "ImPlot"};
    FileDialogDemo FileDialog{this, "FileDialog"};
};

struct Metrics : Window {
    using Window::Window;
    void Draw() const override;

    struct FlowGridMetrics : UIStateMember {
        using UIStateMember::UIStateMember;
        void Draw() const override;
        Bool ShowRelativePaths{this, "ShowRelativePaths", true};
    };
    struct ImGuiMetrics : UIStateMember {
        using UIStateMember::UIStateMember;
        void Draw() const override;
    };
    struct ImPlotMetrics : UIStateMember {
        using UIStateMember::UIStateMember;
        void Draw() const override;
    };

    FlowGridMetrics FlowGrid{this, "FlowGrid"};
    ImGuiMetrics ImGui{this, "ImGui"};
    ImPlotMetrics ImPlot{this, "ImPlot"};
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

string to_string(IO io, bool shorten = false);

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

    void Draw() const override;

    struct FaustState : StateMember {
        using StateMember::StateMember;

        struct FaustEditor : Window {
            using Window::Window;
            void Draw() const override;

            string FileName{"default.dsp"}; // todo state member & respond to changes, or remove from state
        };

        struct FaustDiagram : Window {
            using Window::Window;
            void Draw() const override;

            struct DiagramSettings : StateMember {
                using StateMember::StateMember;
                Flags HoverFlags{
                    this, "HoverFlags",
                    {"ShowRect?Display the hovered node's bounding rectangle",
                     "ShowType?Display the hovered node's box type",
                     "ShowChannels?Display the hovered node's channel points and indices",
                     "ShowChildChannels?Display the channel points and indices for each of the hovered node's children"},
                    FaustDiagramHoverFlags_None,
                    "?Hovering over a node in the graph will display the selected information",
                };
            };

            DiagramSettings Settings{this, "Settings"};
        };

        struct FaustParams : Window {
            using Window::Window;
            void Draw() const override;
        };

        // todo move to top-level Log
        struct FaustLog : Window {
            using Window::Window;
            void Draw() const override;
        };

        FaustEditor Editor{this, "Editor", "Faust editor"};
        FaustDiagram Diagram{this, "Diagram", "Faust diagram"};
        FaustParams Params{this, "Params", "Faust params"};
        FaustLog Log{this, "Log", "Faust log"};

//        String Code{this, "Code", "Code", R"#(import("stdfaust.lib");
//pitchshifter = vgroup("Pitch Shifter", ef.transpose(
//    vslider("window (samples)", 1000, 50, 10000, 1),
//    vslider("xfade (samples)", 10, 1, 10000, 1),
//    vslider("shift (semitones)", 0, -24, +24, 0.1)
//  )
//);
//process = _ : pitchshifter;)#"};
//        String Code{this, "Code", "Code", R"#(import("stdfaust.lib");
//s = vslider("Signal[style:radio{'Noise':0;'Sawtooth':1}]",0,0,1,1);
//process = select2(s,no.noise,os.sawtooth(440));)#"};
//        String Code{this, "Code", "Code", R"(import("stdfaust.lib");
//process = ba.beat(240) : pm.djembe(60, 0.3, 0.4, 1) <: dm.freeverb_demo;)"};
//        String Code{this, "Code", "Code", R"(import("stdfaust.lib");
//process = _:fi.highpass(2,1000):_;)"};
//        String Code{this, "Code", "Code", R"(import("stdfaust.lib");
//ctFreq = hslider("cutoffFrequency",500,50,10000,0.01);
//q = hslider("q",5,1,30,0.1);
//gain = hslider("gain",1,0,1,0.01);
//process = no:noise : fi.resonlp(ctFreq,q,gain);)"};

// Based on Faust::UITester.dsp
        String Code{this, "Code", "Code", R"#(import("stdfaust.lib");
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
    const String &get_device_id(IO io) const { return io == IO_In ? InDeviceId : OutDeviceId; }

    Bool FaustRunning{this, "FaustRunning", true, "?Disabling completely skips Faust computation when computing audio output."};
    Bool Muted{this, "Muted", true, "?Enabling sets all audio output to zero.\nAll audio computation will still be performed, so this setting does not affect CPU load."};
    AudioBackend Backend = none;
    String InDeviceId{this, "InDeviceId", "In device ID"};
    String OutDeviceId{this, "OutDeviceId", "Out device ID"};
    Int InSampleRate{this, "InSampleRate"};
    Int OutSampleRate{this, "OutSampleRate"};
    Enum InFormat{this, "InFormat", {"Invalid", "Float64", "Float32", "Short32", "Short16"}, IoFormat_Invalid};
    Enum OutFormat{this, "OutFormat", {"Invalid", "Float64", "Float32", "Short32", "Short16"}, IoFormat_Invalid};
    Float OutDeviceVolume{this, "OutDeviceVolume", 1.0};
    Bool MonitorInput{this, "MonitorInput", false, "?Enabling adds the audio input stream directly to the audio output."};

    FaustState Faust{this, "Faust"};
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
    struct FileDialog : DialogData, UIStateMember {
        FileDialog(const StateMember *parent, const string &path_segment)
            : DialogData(), UIStateMember(parent, path_segment, Title) {}

        FileDialog &operator=(const DialogData &other) {
            DialogData::operator=(other);
            Visible = true;
            return *this;
        }

        void Draw() const override;
    };

    FileDialog Dialog{this, "Dialog"};
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
    // Box fill colors of various types. todo design these colors for Dark/Classic/Light profiles
    FlowGridCol_DiagramNormal,
    FlowGridCol_DiagramUi,
    FlowGridCol_DiagramSlot,
    FlowGridCol_DiagramNumber,
    // Params colors.
    FlowGridCol_ParamsBg, // ImGuiCol_FrameBg with less alpha

    FlowGridCol_COUNT
};
using FlowGridCol = int;

struct Style : Window {
    using Window::Window;

    void Draw() const override;

    struct FlowGridStyle : UIStateMember {
        FlowGridStyle(const StateMember *parent, const string &path_segment, const string &name = "") : UIStateMember(parent, path_segment, name) {
            ColorsDark();
            DiagramColorsDark();
            DiagramLayoutFlowGrid();
        }

        void Draw() const override;

        ImVec4 Colors[FlowGridCol_COUNT];
        Float FlashDurationSec{this, "FlashDurationSec", 0.6, 0, 5};

        Int DiagramFoldComplexity{
            this, "DiagramFoldComplexity", 3, 0, 20,
            "?Number of boxes within a diagram before folding into a sub-diagram.\n"
            "Setting to zero disables folding altogether, for a fully-expanded diagram."};
        Bool DiagramScaleLinked{this, "DiagramScaleLinked", true, "?Link X/Y"}; // Link X/Y scale sliders, forcing them to the same value.
        Bool DiagramScaleFill{
            this, "DiagramScaleFill", false,
            "?Scale to fill the window.\n"
            "Enabling this setting deactivates other diagram scale settings."};
        Vec2 DiagramScale{this, "DiagramScale", {1, 1}, 0.1, 10};
        Enum DiagramDirection{this, "DiagramDirection", {"Left", "Right"}, ImGuiDir_Right};
        Bool DiagramRouteFrame{this, "DiagramRouteFrame", false};
        Bool DiagramSequentialConnectionZigzag{this, "DiagramSequentialConnectionZigzag", true}; // false allows for diagonal lines instead of zigzags instead of zigzags
        Bool DiagramOrientationMark{this, "DiagramOrientationMark", true};
        Float DiagramOrientationMarkRadius{this, "DiagramOrientationMarkRadius", 1.5, 0.5, 3};
        Float DiagramTopLevelMargin{this, "DiagramTopLevelMargin", 20, 0, 40};
        Float DiagramDecorateMargin{this, "DiagramDecorateMargin", 20, 0, 40};
        Float DiagramDecorateLineWidth{this, "DiagramDecorateLineWidth", 1, 0, 4};
        Float DiagramDecorateCornerRadius{this, "DiagramDecorateCornerRadius", 0, 0, 10};
        Float DiagramBoxCornerRadius{this, "DiagramBoxCornerRadius", 0, 0, 10};
        Float DiagramBinaryHorizontalGapRatio{this, "DiagramBinaryHorizontalGapRatio", 0.25, 0, 1};
        Float DiagramWireWidth{this, "DiagramWireWidth", 1, 0.5, 4};
        Float DiagramWireGap{this, "DiagramWireGap", 16, 10, 20};
        Vec2 DiagramGap{this, "DiagramGap", {8, 8}, 0, 20};
        Vec2 DiagramArrowSize{this, "DiagramArrowSize", {3, 2}, 1, 10};
        Float DiagramInverterRadius{this, "DiagramInverterRadius", 3, 1, 5};

        Bool ParamsHeaderTitles{this, "ParamsHeaderTitles", true};
        Float ParamsMinHorizontalItemWidth{this, "ParamsMinHorizontalItemWidth", 4, 2, 8}; // In frame-height units
        Float ParamsMaxHorizontalItemWidth{this, "ParamsMaxHorizontalItemWidth", 16, 10, 24}; // In frame-height units
        Float ParamsMinVerticalItemHeight{this, "ParamsMinVerticalItemHeight", 4, 2, 8}; // In frame-height units
        Float ParamsMinKnobItemSize{this, "ParamsMinKnobItemSize", 3, 2, 6}; // In frame-height units
        Enum ParamsAlignmentHorizontal{this, "ParamsAlignmentHorizontal", {"Left", "Center", "Right"}, HAlign_Center};
        Enum ParamsAlignmentVertical{this, "ParamsAlignmentVertical", {"Top", "Center", "Bottom"}, VAlign_Center};
        Flags ParamsTableFlags{this, "ParamsTableFlags", TableFlagItems, TableFlags_Borders | TableFlags_Reorderable | TableFlags_Hideable};
        Enum ParamsWidthSizingPolicy{
            this, "ParamsWidthSizingPolicy", {"StretchToFill", "StretchFlexibleOnly", "Balanced"}, ParamsWidthSizingPolicy_StretchFlexibleOnly,
            "?StretchFlexibleOnly: If a table contains only fixed-width items, it won't stretch to fill available width.\n"
            "StretchToFill: If a table contains only fixed-width items, allow columns to stretch to fill available width.\n"
            "Balanced: All param types are given flexible-width, weighted by their minimum width. (Looks more balanced, but less expansion room for wide items).\n"};

        void ColorsDark() {
            Colors[FlowGridCol_HighlightText] = {1, 0.6, 0, 1};
            Colors[FlowGridCol_GestureIndicator] = {0.87, 0.52, 0.32, 1};
            Colors[FlowGridCol_ParamsBg] = {0.16, 0.29, 0.48, 0.1};
        }
        void ColorsClassic() {
            Colors[FlowGridCol_HighlightText] = {1, 0.6, 0, 1};
            Colors[FlowGridCol_GestureIndicator] = {0.87, 0.52, 0.32, 1};
            Colors[FlowGridCol_ParamsBg] = {0.43, 0.43, 0.43, 0.1};
        }
        void ColorsLight() {
            Colors[FlowGridCol_HighlightText] = {1, 0.45, 0, 1};
            Colors[FlowGridCol_GestureIndicator] = {0.87, 0.52, 0.32, 1};
            Colors[FlowGridCol_ParamsBg] = {1, 1, 1, 1};
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
                case FlowGridCol_ParamsBg: return "ParamsBg";
                default: return "Unknown";
            }
        }
    };
    struct ImGuiStyle : UIStateMember {
        ImGuiStyle(const StateMember *parent, const string &path_segment, const string &name = "")
            : UIStateMember(parent, path_segment, name) {
            ImGui::StyleColorsDark(Colors);
        }

        void Apply(ImGuiContext *ctx) const;
        void Draw() const override;

        // See `ImGui::ImGuiStyle` for field descriptions.
        // Initial values copied from `ImGui::ImGuiStyle()` default constructor.
        // Ranges copied from `ImGui::StyleEditor`.
        // Double-check everything's up-to-date from time to time!
        Float Alpha{this, "Alpha", 1, 0.2, 1}; // Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets).
        Float DisabledAlpha{this, "DisabledAlpha", 0.6, 0, 1, "?Additional alpha multiplier for disabled items (multiply over current value of Alpha)."};
        Vec2 WindowPadding{this, "WindowPadding", {8, 8}, 0, 20};
        Float WindowRounding{this, "WindowRounding", 0, 0, 12};
        Float WindowBorderSize{this, "WindowBorderSize", 1};
        Vec2 WindowMinSize{this, "WindowMinSize", {32, 32}};
        Vec2 WindowTitleAlign{this, "WindowTitleAlign", {0, 0.5}};
        Enum WindowMenuButtonPosition{this, "WindowMenuButtonPosition", {"Left", "Right"}, ImGuiDir_Left};
        Float ChildRounding{this, "ChildRounding", 0, 0, 12};
        Float ChildBorderSize{this, "ChildBorderSize", 1};
        Float PopupRounding{this, "PopupRounding", 0, 0, 12};
        Float PopupBorderSize{this, "PopupBorderSize", 1};
        Vec2 FramePadding{this, "FramePadding", {4, 3}, 0, 20};
        Float FrameRounding{this, "FrameRounding", 0, 0, 12};
        Float FrameBorderSize{this, "FrameBorderSize", 0};
        Vec2 ItemSpacing{this, "ItemSpacing", {8, 4}, 0, 20};
        Vec2 ItemInnerSpacing{this, "ItemInnerSpacing", {4, 4}, 0, 20};
        Vec2 CellPadding{this, "CellPadding", {4, 2}, 0, 20};
        Vec2 TouchExtraPadding{this, "TouchExtraPadding", {0, 0}, 0, 10};
        Float IndentSpacing{this, "IndentSpacing", 21, 0, 30};
        Float ColumnsMinSpacing{this, "ColumnsMinSpacing", 6};
        Float ScrollbarSize{this, "ScrollbarSize", 14, 1, 20};
        Float ScrollbarRounding{this, "ScrollbarRounding", 9, 0, 12};
        Float GrabMinSize{this, "GrabMinSize", 12, 1, 20};
        Float GrabRounding{this, "GrabRounding", 0, 0, 12};
        Float LogSliderDeadzone{this, "LogSliderDeadzone", 4, 0, 12};
        Float TabRounding{this, "TabRounding", 4, 0, 12};
        Float TabBorderSize{this, "TabBorderSize", 0};
        Float TabMinWidthForCloseButton{this, "TabMinWidthForCloseButton", 0};
        Enum ColorButtonPosition{this, "ColorButtonPosition", {"Left", "Right"}, ImGuiDir_Right};
        Vec2 ButtonTextAlign{this, "ButtonTextAlign", {0.5, 0.5}, 0, 1, "?Alignment applies when a button is larger than its text content."};
        Vec2 SelectableTextAlign{this, "SelectableTextAlign", {0, 0}, 0, 1, "?Alignment applies when a selectable is larger than its text content."};
        Vec2 DisplayWindowPadding{this, "DisplayWindowPadding", {19, 19}};
        Vec2 DisplaySafeAreaPadding{this, "DisplaySafeAreaPadding", {3, 3}, 0, 30, "?Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured)."};
        Float MouseCursorScale{this, "MouseCursorScale", 1};
        Bool AntiAliasedLines{this, "AntiAliasedLines", true, "Anti-aliased lines?When disabling anti-aliasing lines, you'll probably want to disable borders in your style as well."};
        Bool AntiAliasedLinesUseTex{this, "AntiAliasedLinesUseTex", true,
                                    "Anti-aliased lines use texture?Faster lines using texture data. Require backend to render with bilinear filtering (not point/nearest filtering)."};
        Bool AntiAliasedFill{this, "AntiAliasedFill", true, "Anti-aliased fill"};
        Float CurveTessellationTol{this, "CurveTessellationTol", 1.25, 0.1, 10, "Curve tesselation tolerance"};
        Float CircleTessellationMaxError{this, "CircleTessellationMaxError", 0.3, 0.1, 5};
        ImVec4 Colors[ImGuiCol_COUNT];
    };
    struct ImPlotStyle : UIStateMember {
        ImPlotStyle(const StateMember *parent, const string &path_segment, const string &name = "")
            : UIStateMember(parent, path_segment, name) {
            Colormap = ImPlotColormap_Deep;
            ImPlot::StyleColorsAuto(Colors);
        }

        void Apply(ImPlotContext *ctx) const;
        void Draw() const override;

        // See `ImPlotStyle` for field descriptions.
        // Initial values copied from `ImPlotStyle()` default constructor.
        // Ranges copied from `ImPlot::StyleEditor`.
        // Double-check everything's up-to-date from time to time!
        Float LineWeight{this, "LineWeight", 1, 0, 5};
        Int Marker{this, "Marker", ImPlotMarker_None};
        Float MarkerSize{this, "MarkerSize", 4, 2, 10};
        Float MarkerWeight{this, "MarkerWeight", 1, 0, 5};
        Float FillAlpha{this, "FillAlpha", 1};
        Float ErrorBarSize{this, "ErrorBarSize", 5, 0, 10};
        Float ErrorBarWeight{this, "ErrorBarWeight", 1.5, 0, 5};
        Float DigitalBitHeight{this, "DigitalBitHeight", 8, 0, 20};
        Float DigitalBitGap{this, "DigitalBitGap", 4, 0, 20};
        Float PlotBorderSize{this, "PlotBorderSize", 1, 0, 2};
        Float MinorAlpha{this, "MinorAlpha", 0.25};
        Vec2 MajorTickLen{this, "MajorTickLen", {10, 10}, 0, 20};
        Vec2 MinorTickLen{this, "MinorTickLen", {5, 5}, 0, 20};
        Vec2 MajorTickSize{this, "MajorTickSize", {1, 1}, 0, 2};
        Vec2 MinorTickSize{this, "MinorTickSize", {1, 1}, 0, 2};
        Vec2 MajorGridSize{this, "MajorGridSize", {1, 1}, 0, 2};
        Vec2 MinorGridSize{this, "MinorGridSize", {1, 1}, 0, 2};
        Vec2 PlotPadding{this, "PlotPadding", {10, 10}, 0, 20};
        Vec2 LabelPadding{this, "LabelPadding", {5, 5}, 0, 20};
        Vec2 LegendPadding{this, "LegendPadding", {10, 10}, 0, 20};
        Vec2 LegendInnerPadding{this, "LegendInnerPadding", {5, 5}, 0, 10};
        Vec2 LegendSpacing{this, "LegendSpacing", {5, 0}, 0, 5};
        Vec2 MousePosPadding{this, "MousePosPadding", {10, 10}, 0, 20};
        Vec2 AnnotationPadding{this, "AnnotationPadding", {2, 2}, 0, 5};
        Vec2 FitPadding{this, "FitPadding", {0, 0}, 0, 0.2};
        Vec2 PlotDefaultSize{this, "PlotDefaultSize", {400, 300}, 0, 1000};
        Vec2 PlotMinSize{this, "PlotMinSize", {200, 150}, 0, 300};
        ImVec4 Colors[ImPlotCol_COUNT];
        ImPlotColormap Colormap;
        Bool UseLocalTime{this, "UseLocalTime"};
        Bool UseISO8601{this, "UseISO8601"};
        Bool Use24HourClock{this, "Use24HourClock"};
    };

    ImGuiStyle ImGui{this, "ImGui", "?Configure style for base UI"};
    ImPlotStyle ImPlot{this, "ImPlot", "?Configure style for plots"};
    FlowGridStyle FlowGrid{this, "FlowGrid", "?Configure application-specific style"};
};

struct Processes : StateMember {
    using StateMember::StateMember;

    Process UI{this, "UI"};
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

// ImGui exposes the `ImGuiTableColumnSettings` definition in `imgui_internal.h`.
// However, its `SortDirection`, `IsEnabled` & `IsStretch` members are defined as bitfields (e.g. `ImU8 SortDirection : 2`),
// and I can't figure out how to JSON-encode/decode those.
// This definition is the same, but using
struct TableColumnSettings {
    float WidthOrWeight;
    ImGuiID UserID;
    ImGuiTableColumnIdx Index;
    ImGuiTableColumnIdx DisplayOrder;
    ImGuiTableColumnIdx SortOrder;
    ImU8 SortDirection;
    bool IsEnabled; // "Visible" in ini file
    bool IsStretch;

    TableColumnSettings() = default;
    TableColumnSettings(const ImGuiTableColumnSettings &tcs)
        : WidthOrWeight(tcs.WidthOrWeight), UserID(tcs.UserID), Index(tcs.Index), DisplayOrder(tcs.DisplayOrder),
          SortOrder(tcs.SortOrder), SortDirection(tcs.SortDirection), IsEnabled(tcs.IsEnabled), IsStretch(tcs.IsStretch) {}
};

struct TableSettings {
    ImGuiTableSettings Table;
    std::vector<TableColumnSettings> Columns;
};

struct ImGuiSettingsData {
    ImGuiSettingsData() = default;
    explicit ImGuiSettingsData(ImGuiContext *ctx);

    ImVector<ImGuiDockNodeSettings> Nodes;
    ImVector<ImGuiWindowSettings> Windows;
    std::vector<TableSettings> Tables;
};

struct ImGuiSettings : StateMember, ImGuiSettingsData {
    ImGuiSettings(const StateMember *parent, const string &path_segment, const string &name = "")
        : StateMember(parent, path_segment, name), ImGuiSettingsData() {}

    ImGuiSettings &operator=(const ImGuiSettingsData &other) {
        ImGuiSettingsData::operator=(other);
        return *this;
    }

    // Inverse of above constructor. `imgui_context.settings = this`
    // Should behave just like `ImGui::LoadIniSettingsFromMemory`, but using the structured `...Settings` members
    // in this struct instead of the serialized .ini text format.
    void Apply(ImGuiContext *ctx) const;
};

struct Info : Window {
    using Window::Window;
    void Draw() const override;
};

struct StackTool : Window {
    using Window::Window;
    void Draw() const override;
};

struct DebugLog : Window {
    using Window::Window;
    void Draw() const override;
};

// Types for [json-patch](https://jsonpatch.com)
// For a much more well-defined schema, see https://json.schemastore.org/json-patch.

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

//-----------------------------------------------------------------------------
// [SECTION] Actions
//-----------------------------------------------------------------------------

/**
 An `Action` is an immutable representation of a user interaction event.
 Each action stores all information needed to apply the action to the global `State` instance.

 Conventions:
 * Static members
   - _Not relevant as there aren't any static members atm, but note to future-self._
   - Any static members should be prefixed with an underscore to avoid name collisions with data members.
   - All such static members are declared _after_ data members, to allow for default construction using only non-static members.
   - Note that adding static members does not increase the size of the parent `Action` variant.
     (You can verify this by looking at the 'Action variant size' in the FlowGrid metrics window.)
 * Large action structs
   - Use JSON types for actions that hold very large structured data.
     An `Action` is a `std::variant`, which can hold any type, and thus must be large enough to hold its largest type.
*/

// Utility to make a variant visitor out of lambdas, using the "overloaded pattern" described
// [here](https://en.cppreference.com/w/cpp/utility/variant/visit).
template<class... Ts>
struct visitor : Ts ... {
    using Ts::operator()...;
};

template<class... Ts> visitor(Ts...)->visitor<Ts...>;

namespace Actions {
struct undo {};
struct redo {};
struct set_diff_index { int diff_index; };

struct open_project { string path; };
struct open_empty_project {};
struct open_default_project {};
struct show_open_project_dialog {};

struct open_file_dialog { File::DialogData dialog; }; // todo store as json and check effect on action size
struct close_file_dialog {};

struct save_project { string path; };
struct save_current_project {};
struct save_default_project {};
struct show_save_project_dialog {};

struct close_application {};

struct set_value { JsonPath path; json value; };
struct set_values { std::map<JsonPath, json> values; };
//struct patch_value { JsonPatch patch; };
struct toggle_value { JsonPath path; };

struct set_imgui_color_style { int id; };
struct set_implot_color_style { int id; };
struct set_flowgrid_color_style { int id; };
struct set_flowgrid_diagram_color_style { int id; };
struct set_flowgrid_diagram_layout_style { int id; };

struct show_open_faust_file_dialog {};
struct show_save_faust_file_dialog {};
struct show_save_faust_svg_file_dialog {};
struct save_faust_file { string path; };
struct open_faust_file { string path; };
struct save_faust_svg_file { string path; };
} // End `Action` namespace

using namespace Actions;

using Action = std::variant<
    undo, redo, set_diff_index,
    open_project, open_empty_project, open_default_project, show_open_project_dialog,
    save_project, save_default_project, save_current_project, show_save_project_dialog,
    open_file_dialog, close_file_dialog,
    close_application,

    set_value, set_values, toggle_value,

    set_imgui_color_style, set_implot_color_style, set_flowgrid_color_style, set_flowgrid_diagram_color_style,
    set_flowgrid_diagram_layout_style,

    show_open_faust_file_dialog, show_save_faust_file_dialog, show_save_faust_svg_file_dialog,
    open_faust_file, save_faust_file, save_faust_svg_file
>;

namespace action {
using ID = size_t;

using Gesture = std::vector<Action>;
using Gestures = std::vector<Gesture>;

// Default-construct an action by its variant index (which is also its `ID`).
// From https://stackoverflow.com/a/60567091/780425
template<ID I = 0>
Action create(ID index) {
    if constexpr (I >= std::variant_size_v<Action>) throw std::runtime_error{"Action index " + std::to_string(I + index) + " out of bounds"};
    else return index == 0 ? Action{std::in_place_index<I>} : create<I + 1>(index - 1);
}

// Get the index for action variant type.
// From https://stackoverflow.com/a/66386518/780425

#include "Boost/mp11/mp_find.h"

// E.g. `const ActionId action_id = id<action>`
// An action's ID is its index in the `Action` variant.
// Down the road, this means `Action` would need to be append-only (no order changes) for backwards compatibility.
// Not worried about that right now, since that should be an easy piece to replace with some uuid system later.
// Index is simplest.
template<typename T>
constexpr size_t id = mp_find<Action, T>::value;

#define ActionName(action_var_name) snake_case_to_sentence_case(#action_var_name)

// todo find a performant way to not compile if not exhaustive.
//  Could use a visitor on the action...
const std::map<ID, string> name_for_id{
    {id<undo>, ActionName(undo)},
    {id<redo>, ActionName(redo)},
    {id<set_diff_index>, ActionName(set_diff_index)},

    {id<open_project>, ActionName(open_project)},
    {id<open_empty_project>, ActionName(open_empty_project)},
    {id<open_default_project>, ActionName(open_default_project)},
    {id<show_open_project_dialog>, ActionName(show_open_project_dialog)},

    {id<open_file_dialog>, ActionName(open_file_dialog)},
    {id<close_file_dialog>, ActionName(close_file_dialog)},

    {id<save_project>, ActionName(save_project)},
    {id<save_default_project>, ActionName(save_default_project)},
    {id<save_current_project>, ActionName(save_current_project)},
    {id<show_save_project_dialog>, ActionName(show_save_project_dialog)},

    {id<close_application>, ActionName(close_application)},

    {id<set_value>, ActionName(set_value)},
    {id<set_values>, ActionName(set_values)},
    {id<toggle_value>, ActionName(toggle_value)},
//    {id<patch_value>,              ActionName(patch_value)},

    {id<set_imgui_color_style>, "Set ImGui color style"},
    {id<set_implot_color_style>, "Set ImPlot color style"},
    {id<set_flowgrid_color_style>, "Set FlowGrid color style"},
    {id<set_flowgrid_diagram_color_style>, "Set FlowGrid diagram color style"},
    {id<set_flowgrid_diagram_color_style>, "Set FlowGrid diagram layout style"},

    {id<show_open_faust_file_dialog>, "Show open Faust file dialog"},
    {id<show_save_faust_file_dialog>, "Show save Faust file dialog"},
    {id<show_save_faust_svg_file_dialog>, "Show save Faust SVG file dialog"},
    {id<open_faust_file>, "Open Faust file"},
    {id<save_faust_file>, "Save Faust file"},
    {id<save_faust_svg_file>, "Save Faust SVG file"},
};

const std::map<ID, string> shortcut_for_id = {
    {id<undo>, "cmd+z"},
    {id<redo>, "shift+cmd+z"},
    {id<open_empty_project>, "cmd+n"},
    {id<show_open_project_dialog>, "cmd+o"},
    {id<save_current_project>, "cmd+s"},
    {id<open_default_project>, "shift+cmd+o"},
    {id<save_default_project>, "shift+cmd+s"},
};

constexpr ID get_id(const Action &action) { return action.index(); }
string get_name(const Action &action);
const char *get_menu_label(ID action_id);

Gesture merge_gesture(const Gesture &);
} // End `action` namespace

using ActionID = action::ID;
using action::Gesture;
using action::Gestures;

//-----------------------------------------------------------------------------
// [SECTION] Main application `State` struct
//-----------------------------------------------------------------------------

struct State : UIStateMember {
    State() : UIStateMember() {}

    void Draw() const override;
    void Update(const Action &); // State is only updated via `context.on_action(action)`

    ImGuiSettings ImGuiSettings{this, "ImGuiSettings", "ImGui settings"};
    Style Style{this, "Style"};
    ApplicationSettings ApplicationSettings{this, "ApplicationSettings", "Application settings"};
    Audio Audio{this, "Audio"};
    Processes Processes{this, "Processes"};
    File File{this, "File"};
    Info Info{this, "Info"};

    Demo Demo{this, "Demo"};
    Metrics Metrics{this, "Metrics"};
    StackTool StackTool{this, "StackTool"};
    DebugLog DebugLog{this, "DebugLog"};

    StateViewer StateViewer{this, "StateViewer", "State viewer"};
    StateMemoryEditor StateMemoryEditor{this, "StateMemoryEditor", "State memory editor"};
    StatePathUpdateFrequency PathUpdateFrequency{this, "PathUpdateFrequency", "State path update frequency"};
    ProjectPreview ProjectPreview{this, "ProjectPreview", "Project preview"};
};

//-----------------------------------------------------------------------------
// [SECTION] Main `Context` class
//-----------------------------------------------------------------------------

const std::map<ProjectFormat, string> ExtensionForProjectFormat{
    {StateFormat, ".fls"},
    {DiffFormat, ".fld"},
    {ActionFormat, ".fla"},
};

// todo derive from above map
const std::map<string, ProjectFormat> ProjectFormatForExtension{
    {ExtensionForProjectFormat.at(StateFormat), StateFormat},
    {ExtensionForProjectFormat.at(DiffFormat), DiffFormat},
    {ExtensionForProjectFormat.at(ActionFormat), ActionFormat},
};

static const std::set<string> AllProjectExtensions = {".fls", ".fld", ".fla"}; // todo derive from map
static const string AllProjectExtensionsDelimited = AllProjectExtensions | views::join(',') | to<string>;
static const string PreferencesFileExtension = ".flp";
static const string FaustDspFileExtension = ".dsp";

static const fs::path InternalPath = ".flowgrid";
static const fs::path EmptyProjectPath = InternalPath / ("empty" + ExtensionForProjectFormat.at(StateFormat));
static const fs::path DefaultProjectPath = InternalPath / ("default" + ExtensionForProjectFormat.at(StateFormat));
static const fs::path PreferencesPath = InternalPath / ("preferences" + PreferencesFileExtension);

class CTree;
typedef CTree *Box;

using UIContextFlags = int;
enum UIContextFlags_ {
    UIContextFlags_None = 0,
    UIContextFlags_ImGuiSettings = 1 << 0,
    UIContextFlags_ImGuiStyle = 1 << 1,
    UIContextFlags_ImPlotStyle = 1 << 2,
};

enum Direction { Forward, Reverse };

struct StateStats {
    struct Plottable {
        std::vector<const char *> labels;
        std::vector<ImU64> values;
    };

    std::vector<JsonPath> latest_updated_paths{};
    std::map<JsonPath, std::vector<TimePoint>> gesture_update_times_for_path{};
    std::map<JsonPath, std::vector<TimePoint>> committed_update_times_for_path{};
    std::map<JsonPath, TimePoint> latest_update_time_for_path{};
    Plottable PathUpdateFrequency;

    void apply_patch(const JsonPatch &patch, TimePoint time, Direction direction, bool is_gesture);

private:
    Plottable create_path_update_frequency_plottable();
};

struct Context {
    Context();
    ~Context();

    static bool is_user_project_path(const fs::path &);
    bool project_has_changes() const;
    void save_empty_project();

    bool clear_preferences();

    json get_project_json(ProjectFormat format = StateFormat);

    void enqueue_action(const Action &);
    void run_queued_actions(bool force_finalize_gesture = false);
    bool action_allowed(ActionID) const;
    bool action_allowed(const Action &) const;

    void clear();

    void update_ui_context(UIContextFlags flags);
    void update_faust_context();

    Preferences preferences;

    UIContext *ui{};
    StateStats state_stats;

    Diffs diffs;
    int diff_index = -1;

    Gesture active_gesture; // uncompressed, uncommitted
    Gestures gestures; // compressed, committed gesture history
    JsonPatch active_gesture_patch;

    std::optional<fs::path> current_project_path;
    size_t project_start_gesture_count = gestures.size();

    ImFont *defaultFont{};
    ImFont *fixedWidthFont{};

    bool is_widget_gesturing{};
    bool has_new_faust_code{};
    TimePoint gesture_start_time{};
    float gesture_time_remaining_sec{};

    // Read-only public shorthand state references:
    const State &s = state;
    const json &sj = state_json;

private:
    void on_action(const Action &); // This is the only method that modifies `state`.
    void finalize_gesture();
    void on_patch(const Action &action, const JsonPatch &patch); // Called after every state-changing action
    void set_diff_index(int diff_index);
    void increment_diff_index(int diff_index_delta);
    void on_set_value(const JsonPath &path);

    // Takes care of all side effects needed to put the app into the provided application state json.
    // This function can be run at any time, but it's not thread-safe.
    // Running it on anything but the UI thread could cause correctness issues or event crash with e.g. a NPE during a concurrent read.
    // This is especially the case when assigning to `state_json`, which is not an atomic operation like assigning to `_state` is.
    void open_project(const fs::path &);
    bool save_project(const fs::path &);
    void set_current_project_path(const fs::path &path);
    bool write_preferences() const;

    State state{};
    std::queue<const Action> queued_actions;
    json state_json, gesture_begin_state_json; // `state_json` always reflects `state`. `gesture_begin_state_json` is only updated on gesture-end (for diff calculation).
    int gesture_begin_diff_index = -1;
};

//-----------------------------------------------------------------------------
// [SECTION] Widgets
//-----------------------------------------------------------------------------

namespace FlowGrid {
void gestured();

void HelpMarker(const char *help);

bool ColorEdit4(const JsonPath &path, ImGuiColorEditFlags flags = 0, const char *label = nullptr);

void MenuItem(ActionID); // For actions with no data members.

enum JsonTreeNodeFlags_ {
    JsonTreeNodeFlags_None = 0,
    JsonTreeNodeFlags_Highlighted = 1 << 0,
    JsonTreeNodeFlags_Disabled = 1 << 1,
    JsonTreeNodeFlags_DefaultOpen = 1 << 2,
};
using JsonTreeNodeFlags = int;

bool JsonTreeNode(const string &label, JsonTreeNodeFlags flags = JsonTreeNodeFlags_None, const char *id = nullptr);

// If `label` is empty, `JsonTree` will simply show the provided json `value` (object/array/raw value), with no nesting.
// For a non-empty `label`:
//   * If the provided `value` is an array or object, it will show as a nested `JsonTreeNode` with `label` as its parent.
//   * If the provided `value` is a raw value (or null), it will show as as '{label}: {value}'.
void JsonTree(const string &label, const json &value, JsonTreeNodeFlags node_flags = JsonTreeNodeFlags_None, const char *id = nullptr);
} // End `FlowGrid` namespace

//-----------------------------------------------------------------------------
// [SECTION] Globals
//-----------------------------------------------------------------------------

/**
 Declare a full name & convenient shorthand for:
 - The global state instance `s`
 - The global state JSON instance `sj`
 - The global context instances `c = context`

The state instances are initialized in `Context` and assigned in `main.cpp`.
The context instance is initialized and and assigned in `main.cpp`.

Usage example:
```cpp
// Get the canonical application audio state:
const Audio &audio = s.Audio; // Or just access the (read-only) `state` members directly

// Get the currently active gesture (collection of actions) from the global application context:
 const Gesture &active_gesture = c.active_gesture;
```
*/

extern const State &s;
extern const json &sj;
extern Context context, &c;

/**
 This is the main action-queue method.
 Providing `flush = true` will run all enqueued actions (including this one) and finalize any open gesture.
 This is useful for running multiple actions in a single frame, without grouping them into a single gesture.
*/

bool q(Action &&a, bool flush = false);
