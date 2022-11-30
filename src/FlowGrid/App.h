#pragma once

/**
 * The main `State` instance fully describes the application at any point in time.
 *
 * The entire codebase has read-only access to the immutable, single source-of-truth application `const State &s` instance,
 * which also provides an immutable `Update(const Action &, TransientState &) const` method, and a `Draw() const` method.
 */
#include <iostream>
#include <list>
#include <map>
#include <queue>
#include <set>

#include "immer/map.hpp"
#include "immer/map_transient.hpp"
#include "nlohmann/json_fwd.hpp"
#include <range/v3/view/iota.hpp>
#include <range/v3/view/map.hpp>

#include "Primitive.h"
#include "UI/UI.h"
#include "UI/Style.h"
#include "Helper/Sample.h"
#include "Helper/String.h"
#include "Helper/File.h"

#include "imgui_internal.h"

namespace FlowGrid {}
namespace fg = FlowGrid;

using namespace StringHelper;

using namespace fmt;
using namespace nlohmann;

/**
An ID is used to uniquely identify something.
## Notable uses

### `StateMember`
A `StateMember` has an `ID id` instance member.
`StateMember::Id` reflects its `StatePath Path`, using `ImHashStr` to calculate its own `Id` using its `parent.Id` as a seed.
In the same way, each segment in `StateMember::Path` is calculated by appending its own `PathSegment` to its parent's `Path`.
This exactly reflects the way ImGui calculates its window/tab/dockspace/etc. ID calculation.
A drawable `UIStateMember` uses its `ID` (which is also an `ImGuiID`) as the ID for the top-level `ImGui` widget rendered during its `Draw` call.
This results in the nice property that we can find any `UIStateMember` instance by calling `StateMember::WithId.contains(ImGui::GetHoveredID())` any time during a `UIStateMember::Draw`.
 */
using ID = ImGuiID;
using StatePath = fs::path;

/**
Redefining [ImGui's scalar data types](https://github.com/ocornut/imgui/blob/master/imgui.h#L223-L232)

This is done in order to:
  * clarify & document the actual meanings of the FlowGrid integer type aliases below, and
  * emphasize the importance of FlowGrid integer types reflecting ImGui types.

If it wasn't important to keep FlowGrid's integer types mapped 1:1 to ImGui's, we would be using
 [C++11's fixed width integer types](https://en.cppreference.com/w/cpp/types/integer) instead.

Make sure to double check once in a blue moon that the ImGui types have not changed!
*/

// todo move to ImVec2ih, or make a new Vec2S16 type
constexpr U32 PackImVec2ih(const ImVec2ih &unpacked) { return (U32(unpacked.x) << 16) + U32(unpacked.y); }
constexpr ImVec2ih UnpackImVec2ih(const U32 packed) { return {S16(U32(packed) >> 16), S16(U32(packed) & 0xffff)}; }

using StoreEntry = std::pair<StatePath, Primitive>;
using StoreEntries = vector<StoreEntry>;

struct StatePathHash {
    auto operator()(const StatePath &p) const noexcept { return fs::hash_value(p); }
};
using Store = immer::map<StatePath, Primitive, StatePathHash>;
using TransientStore = immer::map_transient<StatePath, Primitive, StatePathHash>;

extern const Store &store; // Read-only global for full, read-only canonical application state.

// These are needed to fully define equality comparison for `Primitive`.
constexpr bool operator==(const ImVec2 &lhs, const ImVec2 &rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }
constexpr bool operator==(const ImVec2ih &lhs, const ImVec2ih &rhs) { return lhs.x == rhs.x && lhs.y == rhs.y; }
constexpr bool operator==(const ImVec4 &lhs, const ImVec4 &rhs) { return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w; }

using namespace std::string_literals;
using std::nullopt;
using std::cout, std::cerr;
using std::unique_ptr, std::make_unique;
using std::min, std::max;
using std::map, std::set;

// E.g. '/foo/bar/baz' => 'baz'
inline string PathVariableName(const StatePath &path) { return path.filename(); }
inline string PathLabel(const StatePath &path) { return SnakeCaseToSentenceCase(PathVariableName(path)); }

// Split the string on '?'.
// If there is no '?' in the provided string, the first element will have the full input string and the second element will be an empty string.
// todo don't split on escaped '\?'
static std::pair<string, string> ParseHelpText(const string &str) {
    const auto help_split = str.find_first_of('?');
    const bool found = help_split != string::npos;
    return {found ? str.substr(0, help_split) : str, found ? str.substr(help_split + 1) : ""};
}

struct Preferences {
    std::list<fs::path> RecentlyOpenedPaths;
};

static const StatePath RootPath{"/"};

struct StateMember {
    static map<ID, StateMember *> WithId; // Allows for access of any state member by ImGui ID

    StateMember(const StateMember *parent = nullptr, const string &path_segment = "", const string &name_help = "");
    StateMember(const StateMember *parent, const string &path_segment, const string &name_help, const Primitive &value);
    virtual ~StateMember();

    const StateMember *Parent;
    StatePath Path;
    string PathSegment, Name, Help;
    ID Id;
    // todo add start byte offset relative to state root, and link from state viewer json nodes to memory editor

protected:
    // Helper to display a (?) mark which shows a tooltip when hovered. Similar to the one in `imgui_demo.cpp`.
    void HelpMarker(bool after = true) const;
};

/**
Convenience macros for compactly defining `StateMember` properties.

`Prop` defines a `StateMember` instance member of type `PropType`, with variable name `PropName`, constructing the state member with `this` as a parent,
and store path-segment `"{PropName}"` (string with value the same as the variable name).

`Prop_` is the same as `Prop`, but the second arg for overriding the displayed name (instead of deriving from the `PropName`/path-segment), and/or a help string.
Optionally prefix an info segment in the name string with a '?'.
E.g. to override the name and provide a help string: "Test-member?A state member for testing things."
Or, to use the path segment for the name but provide a help string, omit the name: "?A state member for testing things."
 */
#define Prop_(PropType, PropName, NameHelp, ...) PropType PropName{this, #PropName, NameHelp, __VA_ARGS__}
#define Prop(PropType, PropName, ...) Prop_(PropType, PropName, "", __VA_ARGS__)

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
    Bool(const StateMember *parent, const string &path_segment, const string &name_help, bool value = false)
        : Base(parent, path_segment, name_help, value) {}

    operator bool() const;

    bool Draw() const override;
    bool DrawMenu() const;

private:
    void Toggle() const; // Used in draw methods.
};

struct UInt : Base {
    UInt(const StateMember *parent, const string &path_segment, const string &name_help, U32 value = 0, U32 min = 0, U32 max = 100)
        : Base(parent, path_segment, name_help, value), min(min), max(max) {}

    operator U32() const;
    operator bool() const { return (bool) (U32) *this; }

    bool operator==(int value) const { return int(*this) == value; }

    bool Draw() const override;

    U32 min, max;
};

struct Int : Base {
    Int(const StateMember *parent, const string &path_segment, const string &name_help, int value = 0, int min = 0, int max = 100)
        : Base(parent, path_segment, name_help, value), min(min), max(max) {}

    operator int() const;

    operator bool() const { return bool(int(*this)); }
    operator short() const { return short(int(*this)); }
    operator char() const { return char(int(*this)); }
    operator S8() const { return S8(int(*this)); }

    bool operator==(int value) const { return int(*this) == value; }

    bool Draw() const override;
    bool Draw(const vector<int> &options) const;

    int min, max;
};

struct Float : Base {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Float(const StateMember *parent, const string &path_segment, const string &name_help, float value = 0, float min = 0, float max = 1, const char *fmt = nullptr)
        : Base(parent, path_segment, name_help, value), min(min), max(max), fmt(fmt) {}

    operator float() const;

    bool Draw() const override;
    bool Draw(ImGuiSliderFlags flags) const;
    bool Draw(float v_speed, ImGuiSliderFlags flags) const;

    float min, max;
    const char *fmt;
};

struct String : Base {
    String(const StateMember *parent, const string &path_segment, const string &name_help, const string &value = "")
        : Base(parent, path_segment, name_help, value) {}

    operator string() const;
    operator bool() const;

    bool operator==(const string &) const;

    bool Draw() const override;
    bool Draw(const vector<string> &options) const;
};

struct Enum : Base {
    Enum(const StateMember *parent, const string &path_segment, const string &name_help, vector<string> names, int value = 0)
        : Base(parent, path_segment, name_help, value), names(std::move(names)) {}

    operator int() const;

    bool Draw() const override;
    bool Draw(const vector<int> &options) const;
    bool DrawMenu() const;

    vector<string> names;
};

// todo in state viewer, make `Annotated` label mode expand out each integer flag into a string list
struct Flags : Base {
    struct Item {
        Item(const char *name_and_help) {
            const auto &[name, help] = ParseHelpText(name_and_help);
            Name = name;
            Help = help;
        }

        string Name, Help;
    };

    // All text after an optional '?' character for each name will be interpreted as an item help string.
    // E.g. `{"Foo?Does a thing", "Bar?Does a different thing", "Baz"}`
    Flags(const StateMember *parent, const string &path_segment, const string &name_help, vector<Item> items, int value = 0)
        : Base(parent, path_segment, name_help, value), items(std::move(items)) {}

    operator int() const;

    bool Draw() const override;
    bool DrawMenu() const;

    vector<Item> items;
};

template<typename T>
struct Vector : Base {
    using Base::Base;

    virtual string GetName(Count index) const { return to_string(index); };

    T operator[](Count index) const;
    Count size(const Store &_store = store) const;

    Store Set(Count index, const T &value, const Store &_store = store) const;
    Store Set(const vector<T> &values, const Store &_store = store) const;
    Store Set(const vector<std::pair<int, T>> &, const Store &_store = store) const;

    void Set(Count index, const T &value, TransientStore &) const;
    void Set(const vector<T> &values, TransientStore &) const;
    void Set(const vector<std::pair<int, T>> &, TransientStore &) const;
    void truncate(Count length, TransientStore &) const; // Delete every element after index `length - 1`.

    bool Draw() const override { return false; };
};

// Really a vector of vectors. Inner vectors need not have the same length.
template<typename T>
struct Vector2D : Base {
    using Base::Base;

    virtual string GetName(Count i, Count j) const { return format("{}/{}", i, j); };

    T at(Count i, Count j, const Store &_store = store) const;
    Count size(const TransientStore &) const; // Number of outer vectors

    Store Set(Count i, Count j, const T &value, const Store &_store = store) const;
    void Set(Count i, Count j, const T &value, TransientStore &) const;
    void truncate(Count length, TransientStore &) const; // Delete every outer vector after index `length - 1`.
    void truncate(Count i, Count length, TransientStore &) const; // Delete every element after index `length - 1` in inner vector `i`.

    bool Draw() const override { return false; };
};

struct Colors : Vector<U32> {
    // An arbitrary transparent color is used to mark colors as "auto".
    // Using a the unique bit pattern `010101` for the RGB components so as not to confuse it with black/white-transparent.
    // Similar to ImPlot's usage of [`IMPLOT_AUTO_COL = ImVec4(0,0,0,-1)`](https://github.com/epezent/implot/blob/master/implot.h#L67),
    // but not using a value so it can be represented in a U32.
    static constexpr U32 AutoColor = 0X00010101;

    Colors(const StateMember *parent, const string &path_segment, const string &name_help, std::function<const char *(int)> get_color_name, const bool allow_auto = false)
        : Vector(parent, path_segment, name_help), AllowAuto(allow_auto), GetColorName(std::move(get_color_name)) {}

    static U32 ConvertFloat4ToU32(const ImVec4 &value) { return value == IMPLOT_AUTO_COL ? AutoColor : ImGui::ColorConvertFloat4ToU32(value); }
    static ImVec4 ConvertU32ToFloat4(const U32 value) { return value == AutoColor ? IMPLOT_AUTO_COL : ImGui::ColorConvertU32ToFloat4(value); }

    string GetName(Count index) const override { return GetColorName(int(index)); };
    bool Draw() const override;

    void Set(const vector<ImVec4> &values, TransientStore &transient) const {
        Vector::Set(values | transform([](const auto &value) { return ConvertFloat4ToU32(value); }) | to<vector>, transient);
    }
    void Set(const vector<std::pair<int, ImVec4>> &entries, TransientStore &transient) const {
        Vector::Set(entries | transform([](const auto &entry) { return std::pair(entry.first, ConvertFloat4ToU32(entry.second)); }) | to<vector>, transient);
    }

private:
    bool AllowAuto;
    const std::function<const char *(int)> GetColorName;
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

static const vector<Flags::Item> TableFlagItems{
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
    Window(const StateMember *parent, const string &path_segment, const string &name_help = "", bool visible = true);

    Prop(Bool, Visible, true);

    ImGuiWindow &FindImGuiWindow() const { return *ImGui::FindWindowByName(Name.c_str()); }
    void DrawWindow(ImGuiWindowFlags flags = ImGuiWindowFlags_None) const;
    void Dock(ID node_id) const;
    bool ToggleMenuItem() const;
    void SelectTab() const;
};

struct ApplicationSettings : Window {
    using Window::Window;

    void Draw() const override;

    Prop(Float, GestureDurationSec, 0.5, 0, 5); // Merge actions occurring in short succession into a single gesture
};

struct StateViewer : Window {
    using Window::Window;
    void Draw() const override;

    enum LabelMode { Annotated, Raw };
    Prop_(Enum, LabelMode, "?The raw JSON state doesn't store keys for all items.\n"
                           "For example, the main `ui.style.colors` state is a list.\n\n"
                           "'Annotated' mode shows (highlighted) labels for such state items.\n"
                           "'Raw' mode shows the state exactly as it is in the raw JSON state.",
        { "Annotated", "Raw" }, Annotated
    );
    Prop_(Bool, AutoSelect, "Auto-Select?When auto-select is enabled, state changes automatically open.\n"
                            "The state viewer to the changed state node(s), closing all other state nodes.\n"
                            "State menu items can only be opened or closed manually if auto-select is disabled.", true);

    void StateJsonTree(const string &key, const json &value, const StatePath &path = RootPath) const;
};

struct StateMemoryEditor : Window {
    using Window::Window;
    void Draw() const override;
};

struct StatePathUpdateFrequency : Window {
    using Window::Window;
    void Draw() const override;
};

enum ProjectFormat { StateFormat, ActionFormat };

struct ProjectPreview : Window {
    using Window::Window;
    void Draw() const override;

    Prop(Enum, Format, { "StateFormat", "ActionFormat" }, 1);
    Prop(Bool, Raw);
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

    Prop(ImGuiDemo, ImGui);
    Prop(ImPlotDemo, ImPlot);
    Prop(FileDialogDemo, FileDialog);
};

struct Metrics : Window {
    using Window::Window;
    void Draw() const override;

    struct FlowGridMetrics : UIStateMember {
        using UIStateMember::UIStateMember;
        void Draw() const override;
        Prop(Bool, ShowRelativePaths, true);
    };
    struct ImGuiMetrics : UIStateMember {
        using UIStateMember::UIStateMember;
        void Draw() const override;
    };
    struct ImPlotMetrics : UIStateMember {
        using UIStateMember::UIStateMember;
        void Draw() const override;
    };

    Prop(FlowGridMetrics, FlowGrid);
    Prop(ImGuiMetrics, ImGui);
    Prop(ImPlotMetrics, ImPlot);
};

enum AudioBackend { none, dummy, alsa, pulseaudio, jack, coreaudio, wasapi };

// Starting at `-1` allows for using `IO` types as array indices.
enum IO_ { IO_None = -1, IO_In, IO_Out };
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

struct Audio : Window {
    using Window::Window;

    // A selection of supported formats, corresponding to `SoundIoFormat`
    enum IoFormat_ {
        IoFormat_Invalid = 0,
        IoFormat_Float64NE,
        IoFormat_Float32NE,
        IoFormat_S32NE,
        IoFormat_S16NE,
    };
    using IoFormat = int;
    static const vector<IoFormat> PrioritizedDefaultFormats;
    static const vector<int> PrioritizedDefaultSampleRates;

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
                Prop_(Flags, HoverFlags,
                    "?Hovering over a node in the graph will display the selected information",
                    {
                        "ShowRect?Display the hovered node's bounding rectangle",
                        "ShowType?Display the hovered node's box type",
                        "ShowChannels?Display the hovered node's channel points and indices",
                        "ShowChildChannels?Display the channel points and indices for each of the hovered node's children"
                    },
                    FaustDiagramHoverFlags_None
                );
            };

            Prop(DiagramSettings, Settings);
        };

        struct FaustParams : Window {
            using Window::Window;
            void Draw() const override;
        };

        struct FaustLog : Window {
            using Window::Window;
            void Draw() const override;
        };

        Prop_(FaustEditor, Editor, "Faust editor");
        Prop_(FaustDiagram, Diagram, "Faust diagram");
        Prop_(FaustParams, Params, "Faust params");
        Prop_(FaustLog, Log, "Faust log");

//        Prop(String, Code, R"#(import("stdfaust.lib");
//pitchshifter = vgroup("Pitch Shifter", ef.transpose(
//    vslider("window (samples)", 1000, 50, 10000, 1),
//    vslider("xfade (samples)", 10, 1, 10000, 1),
//    vslider("shift (semitones)", 0, -24, +24, 0.1)
//  )
//);
//process = _ : pitchshifter;)#");
//        Prop(String, Code, R"#(import("stdfaust.lib");
//s = vslider("Signal[style:radio{'Noise':0;'Sawtooth':1}]",0,0,1,1);
//process = select2(s,no.noise,os.sawtooth(440));)#");
//        Prop(String, Code, R"(import("stdfaust.lib");
//process = ba.beat(240) : pm.djembe(60, 0.3, 0.4, 1) <: dm.freeverb_demo;)");
//        Prop(String, Code, R"(import("stdfaust.lib");
//process = _:fi.highpass(2,1000):_;)");
//        Prop(String, Code, R"(import("stdfaust.lib");
//ctFreq = hslider("cutoffFrequency",500,50,10000,0.01);
//q = hslider("q",5,1,30,0.1);
//gain = hslider("gain",1,0,1,0.01);
//process = no:noise : fi.resonlp(ctFreq,q,gain);");

// Based on Faust::UITester.dsp
        Prop(String, Code, R"#(import("stdfaust.lib");
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
    hmisc);)#");
        Prop(String, Error);
    };

    void UpdateProcess() const;
    const String &GetDeviceId(IO io) const { return io == IO_In ? InDeviceId : OutDeviceId; }

    AudioBackend Backend = none;
    Prop_(Bool, Running, format("?Disabling ends the {} process.\nEnabling will start the process up again.", Lowercase(Name)), true);
    Prop_(Bool, FaustRunning, "?Disabling skips Faust computation when computing audio output.", true);
    Prop_(Bool, Muted, "?Enabling sets all audio output to zero.\nAll audio computation will still be performed, so this setting does not affect CPU load.", true);
    Prop(String, InDeviceId);
    Prop(String, OutDeviceId);
    Prop(Int, InSampleRate);
    Prop(Int, OutSampleRate);
    Prop(Enum, InFormat, { "Invalid", "Float64", "Float32", "Short32", "Short16" }, IoFormat_Invalid);
    Prop(Enum, OutFormat, { "Invalid", "Float64", "Float32", "Short32", "Short16" }, IoFormat_Invalid);
    Prop(Float, OutDeviceVolume, 1.0);
    Prop_(Bool, MonitorInput, "?Enabling adds the audio input stream directly to the audio output.");
    Prop(FaustState, Faust);
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

struct Vec2 : UIStateMember {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Vec2(const StateMember *parent, const string &path_segment, const string &name_help,
         const ImVec2 &value = {0, 0}, float min = 0, float max = 1, const char *fmt = nullptr)
        : UIStateMember(parent, path_segment, name_help),
          X(this, "X", "", value.x, min, max), Y(this, "Y", "", value.y, min, max),
          min(min), max(max), fmt(fmt) {}

    operator ImVec2() const { return {X, Y}; }

    void Draw() const override;
    virtual bool Draw(ImGuiSliderFlags flags) const;

    Float X, Y;

    float min, max; // todo don't need these anymore, derive from X/Y
    const char *fmt;
};

struct Vec2Linked : Vec2 {
    using Vec2::Vec2;

    void Draw() const override;
    bool Draw(ImGuiSliderFlags flags) const override;

    Prop(Bool, Linked, true);
};

struct Style : Window {
    using Window::Window;

    void Draw() const override;

    struct FlowGridStyle : UIStateMember {
        FlowGridStyle(const StateMember *parent, const string &path_segment, const string &name_help = "");
        void Draw() const override;

        Prop(Float, FlashDurationSec, 0.6, 0.1, 5);

        Prop_(UInt, DiagramFoldComplexity,
            "?Number of boxes within a diagram before folding into a sub-diagram.\n"
            "Setting to zero disables folding altogether, for a fully-expanded diagram.", 3, 0, 20);
        Prop_(Bool, DiagramScaleFill, "?Scale to fill the window.\nEnabling this setting deactivates other diagram scale settings.");
        Prop(Vec2Linked, DiagramScale, { 1, 1 }, 0.5, 5);
        Prop(Enum, DiagramDirection, { "Left", "Right" }, ImGuiDir_Right);
        Prop(Bool, DiagramRouteFrame);
        Prop(Bool, DiagramOrientationMark, true);
        Prop(Bool, DiagramSequentialConnectionZigzag, true); // false allows for diagonal lines instead of zigzags instead of zigzags

        Prop(Bool, DiagramDecorateFoldedNodes, false);
        Prop(Float, DiagramDecorateCornerRadius, 0, 0, 10);
        Prop(Float, DiagramDecorateLineWidth, 1, 0, 4);
        Prop(Vec2Linked, DiagramDecorateMargin, { 10, 10 }, 0, 20);
        Prop(Vec2Linked, DiagramDecoratePadding, { 10, 10 }, 0, 20);

        Prop(Vec2Linked, DiagramGroupMargin, { 8, 8 }, 0, 20);
        Prop(Vec2Linked, DiagramGroupPadding, { 8, 8 }, 0, 20);

        Prop(Float, DiagramOrientationMarkRadius, 1.5, 0.5, 3);
        Prop(Float, DiagramBoxCornerRadius, 0, 0, 10);
        Prop(Float, DiagramBinaryHorizontalGapRatio, 0.25, 0, 1);
        Prop(Float, DiagramWireWidth, 1, 0.5, 4);
        Prop(Float, DiagramWireGap, 16, 10, 20);
        Prop(Vec2, DiagramGap, { 8, 8 }, 0, 20);
        Prop(Vec2, DiagramArrowSize, { 3, 2 }, 1, 10);
        Prop(Float, DiagramInverterRadius, 3, 1, 5);

        Prop(Bool, ParamsHeaderTitles, true);
        // In frame-height units:
        Prop(Float, ParamsMinHorizontalItemWidth, 4, 2, 8);
        Prop(Float, ParamsMaxHorizontalItemWidth, 16, 10, 24);
        Prop(Float, ParamsMinVerticalItemHeight, 4, 2, 8);
        Prop(Float, ParamsMinKnobItemSize, 3, 2, 6);

        Prop(Enum, ParamsAlignmentHorizontal, { "Left", "Center", "Right" }, HJustify_Middle);
        Prop(Enum, ParamsAlignmentVertical, { "Top", "Center", "Bottom" }, VJustify_Middle);
        Prop(Flags, ParamsTableFlags, TableFlagItems, TableFlags_Borders | TableFlags_Reorderable | TableFlags_Hideable);
        Prop_(Enum, ParamsWidthSizingPolicy,
            "?StretchFlexibleOnly: If a table contains only fixed-width items, it won't stretch to fill available width.\n"
            "StretchToFill: If a table contains only fixed-width items, allow columns to stretch to fill available width.\n"
            "Balanced: All param types are given flexible-width, weighted by their minimum width. (Looks more balanced, but less expansion room for wide items).",
            { "StretchToFill", "StretchFlexibleOnly", "Balanced" }, ParamsWidthSizingPolicy_StretchFlexibleOnly);

        Prop(Colors, Colors, GetColorName);

        void ColorsDark(TransientStore &_store) const;
        void ColorsLight(TransientStore &_store) const;
        void ColorsClassic(TransientStore &_store) const;

        void DiagramColorsDark(TransientStore &_store) const;
        void DiagramColorsClassic(TransientStore &_store) const;
        void DiagramColorsLight(TransientStore &_store) const;
        void DiagramColorsFaust(TransientStore &_store) const; // Color Faust diagrams the same way Faust does when it renders to SVG.
        void DiagramLayoutFlowGrid(TransientStore &_store) const;
        void DiagramLayoutFaust(TransientStore &_store) const; // Lay out Faust diagrams the same way Faust does when it renders to SVG.

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
        ImGuiStyle(const StateMember *parent, const string &path_segment, const string &name_help = "");

        void Apply(ImGuiContext *ctx) const;
        void Draw() const override;

        void ColorsDark(TransientStore &) const;
        void ColorsLight(TransientStore &) const;
        void ColorsClassic(TransientStore &) const;

        static constexpr float FontAtlasScale = 2; // We rasterize to a scaled-up texture and scale down the font size globally, for sharper text.

        // See `ImGui::ImGuiStyle` for field descriptions.
        // Initial values copied from `ImGui::ImGuiStyle()` default constructor.
        // Ranges copied from `ImGui::StyleEditor`.
        // Double-check everything's up-to-date from time to time!

        // Main
        Prop(Vec2, WindowPadding, { 8, 8 }, 0, 20, "%.0f");
        Prop(Vec2, FramePadding, { 4, 3 }, 0, 20, "%.0f");
        Prop(Vec2, CellPadding, { 4, 2 }, 0, 20, "%.0f");
        Prop(Vec2, ItemSpacing, { 8, 4 }, 0, 20, "%.0f");
        Prop(Vec2, ItemInnerSpacing, { 4, 4 }, 0, 20, "%.0f");
        Prop(Vec2, TouchExtraPadding, { 0, 0 }, 0, 10, "%.0f");
        Prop(Float, IndentSpacing, 21, 0, 30, "%.0f");
        Prop(Float, ScrollbarSize, 14, 1, 20, "%.0f");
        Prop(Float, GrabMinSize, 12, 1, 20, "%.0f");

        // Borders
        Prop(Float, WindowBorderSize, 1, 0, 1, "%.0f");
        Prop(Float, ChildBorderSize, 1, 0, 1, "%.0f");
        Prop(Float, FrameBorderSize, 0, 0, 1, "%.0f");
        Prop(Float, PopupBorderSize, 1, 0, 1, "%.0f");
        Prop(Float, TabBorderSize, 0, 0, 1, "%.0f");

        // Rounding
        Prop(Float, WindowRounding, 0, 0, 12, "%.0f");
        Prop(Float, ChildRounding, 0, 0, 12, "%.0f");
        Prop(Float, FrameRounding, 0, 0, 12, "%.0f");
        Prop(Float, PopupRounding, 0, 0, 12, "%.0f");
        Prop(Float, ScrollbarRounding, 9, 0, 12, "%.0f");
        Prop(Float, GrabRounding, 0, 0, 12, "%.0f");
        Prop(Float, LogSliderDeadzone, 4, 0, 12, "%.0f");
        Prop(Float, TabRounding, 4, 0, 12, "%.0f");

        // Alignment
        Prop(Vec2, WindowTitleAlign, { 0, 0.5 }, 0, 1, "%.2f");
        Prop(Enum, WindowMenuButtonPosition, { "Left", "Right" }, ImGuiDir_Left);
        Prop(Enum, ColorButtonPosition, { "Left", "Right" }, ImGuiDir_Right);
        Prop_(Vec2, ButtonTextAlign, "?Alignment applies when a button is larger than its text content.", { 0.5, 0.5 }, 0, 1, "%.2f");
        Prop_(Vec2, SelectableTextAlign, "?Alignment applies when a selectable is larger than its text content.", { 0, 0 }, 0, 1, "%.2f");

        // Safe area padding
        Prop_(Vec2, DisplaySafeAreaPadding, "?Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured).", { 3, 3 }, 0, 30, "%.0f");

        // Rendering
        Prop_(Bool, AntiAliasedLines, "Anti-aliased lines?When disabling anti-aliasing lines, you'll probably want to disable borders in your style as well.", true);
        Prop_(Bool, AntiAliasedLinesUseTex, "Anti-aliased lines use texture?Faster lines using texture data. Require backend to render with bilinear filtering (not point/nearest filtering).", true);
        Prop_(Bool, AntiAliasedFill, "Anti-aliased fill", true);
        Prop_(Float, CurveTessellationTol, "Curve tesselation tolerance", 1.25, 0.1, 10, "%.2f");
        Prop(Float, CircleTessellationMaxError, 0.3, 0.1, 5, "%.2f");
        Prop(Float, Alpha, 1, 0.2, 1, "%.2f"); // Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets).
        Prop_(Float, DisabledAlpha, "?Additional alpha multiplier for disabled items (multiply over current value of Alpha).", 0.6, 0, 1, "%.2f");

        // Fonts
        Prop(Int, FontIndex);
        Prop_(Float, FontScale, "?Global font scale (low-quality!)", 1, 0.3, 2, "%.2f"); // todo add flags option and use `ImGuiSliderFlags_AlwaysClamp` here

        // Not editable todo delete?
        Prop(Float, TabMinWidthForCloseButton, 0);
        Prop(Vec2, DisplayWindowPadding, { 19, 19 });
        Prop(Vec2, WindowMinSize, { 32, 32 });
        Prop(Float, MouseCursorScale, 1);
        Prop(Float, ColumnsMinSpacing, 6);

        Prop(Colors, Colors, ImGui::GetStyleColorName);
    };
    struct ImPlotStyle : UIStateMember {
        ImPlotStyle(const StateMember *parent, const string &path_segment, const string &name_help = "");
        void Apply(ImPlotContext *ctx) const;
        void Draw() const override;

        void ColorsAuto(TransientStore &_store) const;
        void ColorsDark(TransientStore &_store) const;
        void ColorsLight(TransientStore &_store) const;
        void ColorsClassic(TransientStore &_store) const;

        // See `ImPlotStyle` for field descriptions.
        // Initial values copied from `ImPlotStyle()` default constructor.
        // Ranges copied from `ImPlot::StyleEditor`.
        // Double-check everything's up-to-date from time to time!

        // Item styling
        Prop(Float, LineWeight, 1, 0, 5, "%.1f");
        Prop(Float, MarkerSize, 4, 2, 10, "%.1f");
        Prop(Float, MarkerWeight, 1, 0, 5, "%.1f");
        Prop(Float, FillAlpha, 1, 0, 1, "%.2f");
        Prop(Float, ErrorBarSize, 5, 0, 10, "%.1f");
        Prop(Float, ErrorBarWeight, 1.5, 0, 5, "%.1f");
        Prop(Float, DigitalBitHeight, 8, 0, 20, "%.1f");
        Prop(Float, DigitalBitGap, 4, 0, 20, "%.1f");

        // Plot styling
        Prop(Float, PlotBorderSize, 1, 0, 2, "%.0f");
        Prop(Float, MinorAlpha, 0.25, 1, 0, "%.2f");
        Prop(Vec2, MajorTickLen, { 10, 10 }, 0, 20, "%.0f");
        Prop(Vec2, MinorTickLen, { 5, 5 }, 0, 20, "%.0f");
        Prop(Vec2, MajorTickSize, { 1, 1 }, 0, 2, "%.1f");
        Prop(Vec2, MinorTickSize, { 1, 1 }, 0, 2, "%.1f");
        Prop(Vec2, MajorGridSize, { 1, 1 }, 0, 2, "%.1f");
        Prop(Vec2, MinorGridSize, { 1, 1 }, 0, 2, "%.1f");
        Prop(Vec2, PlotDefaultSize, { 400, 300 }, 0, 1000, "%.0f");
        Prop(Vec2, PlotMinSize, { 200, 150 }, 0, 300, "%.0f");

        // Plot padding
        Prop(Vec2, PlotPadding, { 10, 10 }, 0, 20, "%.0f");
        Prop(Vec2, LabelPadding, { 5, 5 }, 0, 20, "%.0f");
        Prop(Vec2, LegendPadding, { 10, 10 }, 0, 20, "%.0f");
        Prop(Vec2, LegendInnerPadding, { 5, 5 }, 0, 10, "%.0f");
        Prop(Vec2, LegendSpacing, { 5, 0 }, 0, 5, "%.0f");
        Prop(Vec2, MousePosPadding, { 10, 10 }, 0, 20, "%.0f");
        Prop(Vec2, AnnotationPadding, { 2, 2 }, 0, 5, "%.0f");
        Prop(Vec2, FitPadding, { 0, 0 }, 0, 0.2, "%.2f");

        Prop(Colors, Colors, ImPlot::GetStyleColorName, true);
        Prop(Bool, UseLocalTime);
        Prop(Bool, UseISO8601);
        Prop(Bool, Use24HourClock);

        Prop(Int, Marker, ImPlotMarker_None); // Not editable todo delete?
    };

    Prop_(ImGuiStyle, ImGui, "?Configure style for base UI");
    Prop_(ImPlotStyle, ImPlot, "?Configure style for plots");
    Prop_(FlowGridStyle, FlowGrid, "?Configure application-specific style");
};

struct ImGuiDockNodeSettings;

// These Dock/Window/Table settings are `StateMember` duplicates of those in `imgui.cpp`.
// They are stored here a structs-of-arrays (vs. arrays-of-structs)
// todo These will show up counter-intuitively in the json state viewers.
//  Use Raw/Formatted settings in state viewers to:
//  * convert structs-of-arrays to arrays-of-structs,
//  * unpack positions/sizes
struct DockNodeSettings : StateMember {
    using StateMember::StateMember;
    void Set(const ImVector<ImGuiDockNodeSettings> &, TransientStore &store) const;
    void Apply(ImGuiContext *) const;

    Prop(Vector<ID>, NodeId);
    Prop(Vector<ID>, ParentNodeId);
    Prop(Vector<ID>, ParentWindowId);
    Prop(Vector<ID>, SelectedTabId);
    Prop(Vector<int>, SplitAxis);
    Prop(Vector<int>, Depth);
    Prop(Vector<int>, Flags);
    Prop(Vector<U32>, Pos); // Packed ImVec2ih
    Prop(Vector<U32>, Size); // Packed ImVec2ih
    Prop(Vector<U32>, SizeRef); // Packed ImVec2ih
};

struct WindowSettings : StateMember {
    using StateMember::StateMember;
    void Set(ImChunkStream<ImGuiWindowSettings> &, TransientStore &store) const;
    void Apply(ImGuiContext *) const;

    Prop(Vector<ImGuiID>, ID);
    Prop(Vector<ImGuiID>, ClassId);
    Prop(Vector<ImGuiID>, ViewportId);
    Prop(Vector<ImGuiID>, DockId);
    Prop(Vector<int>, DockOrder);
    Prop(Vector<U32>, Pos); // Packed ImVec2ih
    Prop(Vector<U32>, Size); // Packed ImVec2ih
    Prop(Vector<U32>, ViewportPos); // Packed ImVec2ih
    Prop(Vector<bool>, Collapsed);
};

struct TableColumnSettings : StateMember {
    using StateMember::StateMember;

    // [table_index][column_index]
    Prop(Vector2D<float>, WidthOrWeight);
    Prop(Vector2D<ID>, UserID);
    Prop(Vector2D<int>, Index);
    Prop(Vector2D<int>, DisplayOrder);
    Prop(Vector2D<int>, SortOrder);
    Prop(Vector2D<int>, SortDirection);
    Prop(Vector2D<bool>, IsEnabled); // "Visible" in ini file
    Prop(Vector2D<bool>, IsStretch);
};

struct TableSettings : StateMember {
    using StateMember::StateMember;
    void Set(ImChunkStream<ImGuiTableSettings> &, TransientStore &store) const;
    void Apply(ImGuiContext *) const;

    Prop(Vector<ImGuiID>, ID);
    Prop(Vector<int>, SaveFlags);
    Prop(Vector<float>, RefScale);
    Prop(Vector<Count>, ColumnsCount);
    Prop(Vector<Count>, ColumnsCountMax);
    Prop(Vector<bool>, WantApply);
    Prop(TableColumnSettings, Columns);
};

struct ImGuiSettings : StateMember {
    using StateMember::StateMember;
    Store Set(ImGuiContext *ctx) const;
    // Inverse of above constructor. `imgui_context.settings = this`
    // Should behave just like `ImGui::LoadIniSettingsFromMemory`, but using the structured `...Settings` members
    //  in this struct instead of the serialized .ini text format.
    void Apply(ImGuiContext *ctx) const;

    Prop(DockNodeSettings, Nodes);
    Prop(WindowSettings, Windows);
    Prop(TableSettings, Tables);
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

using ImGuiFileDialogFlags = int;
// Copied from `ImGuiFileDialog` source with a different name to avoid redefinition. Brittle but we can avoid an include this way.
constexpr ImGuiFileDialogFlags FileDialogFlags_ConfirmOverwrite = 1 << 0;
constexpr ImGuiFileDialogFlags FileDialogFlags_Modal = 1 << 9;
constexpr ImGuiFileDialogFlags FileDialogFlags_Default = FileDialogFlags_ConfirmOverwrite | FileDialogFlags_Modal;

struct FileDialogData {
    string title = "Choose file", filters, file_path = ".", default_file_name;
    bool save_mode = false;
    int max_num_selections = 1;
    ImGuiFileDialogFlags flags = FileDialogFlags_Default;
};

struct FileDialog : Window {
    FileDialog(const StateMember *parent, const string &path_segment, const string &name_help = "", const bool visible = false)
        : Window(parent, path_segment, name_help, visible) {}
    void Set(const FileDialogData &data, TransientStore &) const;
    void Draw() const override;

    Prop(Bool, SaveMode); // The same file dialog instance is used for both saving & opening files.
    Prop(Int, MaxNumSelections, 1);
    Prop(Int, Flags, FileDialogFlags_Default);
    Prop(String, Title, "Choose file");
    Prop(String, Filters);
    Prop(String, FilePath, ".");
    Prop(String, DefaultFileName);
};
struct PatchOp {
    enum Type { Add, Remove, Replace, };

    PatchOp::Type Op{};
    std::optional<Primitive> Value{}; // Present for add/replace
    std::optional<Primitive> Old{}; // Present for remove/replace
};
using PatchOps = map<StatePath, PatchOp>;

static constexpr auto AddOp = PatchOp::Type::Add;
static constexpr auto RemoveOp = PatchOp::Type::Remove;
static constexpr auto ReplaceOp = PatchOp::Type::Replace;

struct Patch {
    PatchOps Ops;
    StatePath BasePath{RootPath};

    bool empty() const noexcept { return Ops.empty(); }
};

struct StatePatch {
    Patch Patch;
    TimePoint Time;
};

string to_string(const Primitive &);
string to_string(PatchOp::Type);

//-----------------------------------------------------------------------------
// [SECTION] Actions
//-----------------------------------------------------------------------------

/**
An `Action` is an immutable representation of a user interaction event.
Each action stores all information needed to apply the action to a `Store` instance.
An `ActionMoment` contains an `Action` and the `TimePoint` at which the action happened.

An `Action` is a `std::variant`, which can hold any type, and thus must be large enough to hold its largest type.
- For actions holding very large structured data, using a JSON string is a good approach to keep the `Action` size down
  (at the expense of losing type safety and storing the string contents in heap memory).
- Note that adding static members does not increase the size of the parent `Action` variant.
  (You can verify this by looking at the 'Action variant size' in the Metrics->FlowGrid window.)
*/

// Utility to make a variant visitor out of lambdas, using the "overloaded pattern" described
// [here](https://en.cppreference.com/w/cpp/utility/variant/visit).
template<class... Ts>
struct visitor : Ts ... {
    using Ts::operator()...;
};
template<class... Ts> visitor(Ts...)->visitor<Ts...>;

// Utility to flatten two variants together into one variant.
// From https://stackoverflow.com/a/59251342/780425
template<typename Var1, typename Var2>
struct variant_flat;
template<typename ... Ts1, typename ... Ts2>
struct variant_flat<std::variant<Ts1...>, std::variant<Ts2...>> {
    using type = std::variant<Ts1..., Ts2...>;
};

namespace Actions {
struct undo {};
struct redo {};
struct set_history_index { int index; };

struct open_project { string path; };
struct open_empty_project {};
struct open_default_project {};

struct show_open_project_dialog {};
struct open_file_dialog { string dialog_json; }; // Storing as JSON string instead of the raw struct to reduce variant size. (Raw struct is 120 bytes.)
struct close_file_dialog {};

struct save_project { string path; };
struct save_current_project {};
struct save_default_project {};
struct show_save_project_dialog {};

struct close_application {};

struct set_value { StatePath path; Primitive value; };
struct set_values { StoreEntries values; };
struct toggle_value { StatePath path; };
struct apply_patch { Patch patch; };

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
} // End `Actions` namespace

using namespace Actions;

// Actions that don't directly update state.
// These don't get added to the action/gesture history, since they result in side effects that don't change values in the main state store.
// These are not saved in a FlowGridAction (.fga) project.
using ProjectAction = std::variant<
    undo, redo, set_history_index,
    open_project, open_empty_project, open_default_project,
    save_project, save_default_project, save_current_project, save_faust_file, save_faust_svg_file
>;
using StateAction = std::variant<
    open_file_dialog, close_file_dialog,
    show_open_project_dialog, show_save_project_dialog, show_open_faust_file_dialog, show_save_faust_file_dialog, show_save_faust_svg_file_dialog,
    open_faust_file,

    set_value, set_values, toggle_value, apply_patch,

    set_imgui_color_style, set_implot_color_style, set_flowgrid_color_style, set_flowgrid_diagram_color_style,
    set_flowgrid_diagram_layout_style,

    close_application
>;
using Action = variant_flat<ProjectAction, StateAction>::type;
using ActionID = ID;

// All actions that don't have any member data.
using EmptyAction = std::variant<
    undo,
    redo,
    open_empty_project,
    open_default_project,
    show_open_project_dialog,
    close_file_dialog,
    save_current_project,
    save_default_project,
    show_save_project_dialog,
    close_application,
    show_open_faust_file_dialog,
    show_save_faust_file_dialog,
    show_save_faust_svg_file_dialog
>;

namespace action {

using ActionMoment = std::pair<Action, TimePoint>;
using StateActionMoment = std::pair<StateAction, TimePoint>;
using Gesture = vector<StateActionMoment>;
using Gestures = vector<Gesture>;

// Default-construct an action by its variant index (which is also its `ID`).
// Adapted from: https://stackoverflow.com/a/60567091/780425
template<ID I = 0>
Action Create(ID index) {
    if constexpr (I >= std::variant_size_v<Action>) throw std::runtime_error{"Action index " + to_string(I + index) + " out of bounds"};
    else return index == 0 ? Action{std::in_place_index<I>} : Create<I + 1>(index - 1);
}

#include "../Boost/mp11/mp_find.h"

// E.g. `ActionID action_id = id<action_type>`
// An action's ID is its index in the `Action` variant.
// Down the road, this means `Action` would need to be append-only (no order changes) for backwards compatibility.
// Not worried about that right now, since it should be easy enough to replace with some UUID system later.
// Index is simplest.
// Mp11 approach from: https://stackoverflow.com/a/66386518/780425
template<typename T>
constexpr ActionID id = mp_find<Action, T>::value;

#define ActionName(action_var_name) SnakeCaseToSentenceCase(#action_var_name)

// Note: ActionID here is index within `Action` variant, not the `EmptyAction` variant.
const map <ActionID, string> ShortcutForId = {
    {id<undo>, "cmd+z"},
    {id<redo>, "shift+cmd+z"},
    {id<open_empty_project>, "cmd+n"},
    {id<show_open_project_dialog>, "cmd+o"},
    {id<open_default_project>, "shift+cmd+o"},
    {id<save_current_project>, "cmd+s"},
    {id<show_save_project_dialog>, "shift+cmd+s"},
};

constexpr ActionID GetId(const Action &action) { return action.index(); }
constexpr ActionID GetId(const StateAction &action) { return action.index(); }
constexpr ActionID GetId(const ProjectAction &action) { return action.index(); }

string GetName(const ProjectAction &action);
string GetName(const StateAction &action);
string GetShortcut(const EmptyAction &);
string GetMenuLabel(const EmptyAction &);
Gesture MergeGesture(const Gesture &);
} // End `action` namespace

using action::Gesture;
using action::Gestures;
using action::ActionMoment;
using action::StateActionMoment;

//-----------------------------------------------------------------------------
// [SECTION] Main application `State`
//-----------------------------------------------------------------------------

struct State : UIStateMember {
    State() : UIStateMember() {}

    void Draw() const override;
    void Update(const StateAction &, TransientStore &) const;
    void Apply(UIContext::Flags flags) const;

    struct UIProcess : Window {
        using Window::Window;
        void Draw() const override {}

        Prop_(Bool, Running, format("?Disabling ends the {} process.\nEnabling will start the process up again.", Lowercase(Name)), true);
    };

    Prop(ImGuiSettings, ImGuiSettings);
    Prop(Style, Style);
    Prop(ApplicationSettings, ApplicationSettings);
    Prop(Audio, Audio);
    Prop(UIProcess, UiProcess);
    Prop(FileDialog, FileDialog);
    Prop(Info, Info);

    Prop(Demo, Demo);
    Prop(Metrics, Metrics);
    Prop(StackTool, StackTool);
    Prop(DebugLog, DebugLog);

    Prop(StateViewer, StateViewer);
    Prop(StateMemoryEditor, StateMemoryEditor);
    Prop(StatePathUpdateFrequency, StatePathUpdateFrequency);
    Prop(ProjectPreview, ProjectPreview);
};

namespace FlowGrid {
void MenuItem(const EmptyAction &); // For actions with no data members.
}

//-----------------------------------------------------------------------------
// [SECTION] Main application `Context`
//-----------------------------------------------------------------------------

static const map<ProjectFormat, string> ExtensionForProjectFormat{{StateFormat, ".fls"}, {ActionFormat, ".fla"}};
static const auto ProjectFormatForExtension = ExtensionForProjectFormat |
    transform([](const auto &pair) { return std::pair(pair.second, pair.first); }) | to<map>();
static const auto AllProjectExtensions = views::keys(ProjectFormatForExtension) | to<set>;
static const string AllProjectExtensionsDelimited = AllProjectExtensions | views::join(',') | to<string>;
static const string PreferencesFileExtension = ".flp";
static const string FaustDspFileExtension = ".dsp";

static const fs::path InternalPath = ".flowgrid";
static const fs::path EmptyProjectPath = InternalPath / ("empty" + ExtensionForProjectFormat.at(StateFormat));
// The default project is a user-created project that loads on app start, instead of the empty project.
// As an action-formatted project, it builds on the empty project, replaying the actions present at the time the default project was saved.
static const fs::path DefaultProjectPath = InternalPath / ("default" + ExtensionForProjectFormat.at(ActionFormat));
static const fs::path PreferencesPath = InternalPath / ("Preferences" + PreferencesFileExtension);

enum Direction { Forward, Reverse };

struct StoreHistory {
    struct Record {
        TimePoint Committed;
        Store Store; // The store as it was at `Committed` time
        Gesture Gesture; // Compressed gesture (list of `ActionMoment`s) that caused the store change
    };
    struct Plottable {
        vector<const char *> Labels;
        vector<ImU64> Values;
    };

    StoreHistory(const Store &_store) : Records{{Clock::now(), _store, {}}} {}

    void UpdateGesturePaths(const Gesture &, const Patch &);
    Plottable StatePathUpdateFrequencyPlottable() const;
    std::optional<TimePoint> LatestUpdateTime(const StatePath &path) const;

    void FinalizeGesture();
    void SetIndex(Count);

    Count Size() const;
    bool Empty() const;
    bool CanUndo() const;
    bool CanRedo() const;

    Gestures Gestures() const;
    TimePoint GestureStartTime() const;
    float GestureTimeRemainingSec() const;

    Count Index{0};
    vector<Record> Records;
    Gesture ActiveGesture; // uncompressed, uncommitted

    vector<StatePath> LatestUpdatedPaths{};
    map<StatePath, vector<TimePoint>> CommittedUpdateTimesForPath{};

private:
    map<StatePath, vector<TimePoint>> GestureUpdateTimesForPath{};
};

struct Context {
    Context();
    ~Context();

    static bool IsUserProjectPath(const fs::path &);
    json GetProjectJson(ProjectFormat format = StateFormat);
    void SaveEmptyProject();
    void OpenProject(const fs::path &);
    bool SaveProject(const fs::path &);
    void SaveCurrentProject();

    void RunQueuedActions(bool force_finalize_gesture = false);
    bool ActionAllowed(ID) const;
    bool ActionAllowed(const Action &) const;
    bool ActionAllowed(const EmptyAction &) const;

    bool ClearPreferences();
    void Clear();

    // Main setter to modify the canonical application state store.
    // _All_ store assignments happen via this method.
    Patch SetStore(const Store &new_store);

    TransientStore CtorStore{}; // Used in `StateMember` constructors to initialize the store.

private:
    const State ApplicationState{};
    Store ApplicationStore{CtorStore.persistent()}; // Create the local canonical store, initially containing the full application state constructed by `State`.

public:
    const State &s = ApplicationState;
    const Store &store = ApplicationStore;

    Preferences Preferences;
    StoreHistory History{store}; // One store checkpoint for every gesture.
    bool ProjectHasChanges;

private:
    void ApplyAction(const ProjectAction &);

    void SetCurrentProjectPath(const fs::path &);
    bool WritePreferences() const;

    std::optional<fs::path> CurrentProjectPath;
};

//-----------------------------------------------------------------------------
// [SECTION] Globals
//-----------------------------------------------------------------------------

/**
Declare read-only accessors for:
 - The global state instance `state` (and its shorthand, `s`)
 - The global context instance `context` (and its shorthand, `c`)

The state & context instances are initialized and instantiated in `main.cpp`.

`state`/`s` is a read-only structured representation of its underlying store (of type `Store`, which itself is an `immer::map<Path, Primitive>`).
It provides a full nested struct representation of the state, along with additional metadata about each state member, such as its `Path`/`ID`/`Name`/`Info`.
Basically, it has everything about the state member except its _actual value_ (a `Primitive`, struct of `Primitive`s, or collection of either).
- Immutable assignment operators, which return a modified copy of the value resulting from applying the assignment.
  Note that this is only _conceptually_ a copy, since it's a persistent data structure.
  Typical modifications require very little data to be copied to store its value before the modification.
  See [CppCon 2017: Phil Nash “The Holy Grail! A Hash Array Mapped Trie for C++”](https://youtu.be/imrSQ82dYns) for details on how this is done.
  HAMTs are heavily used in the implementation of Closure.
  Big thanks to my friend Justin Smith for suggesting using HAMTs for an efficient application state tree - they're fantastic for it!
- Values act like the member of `Primitive` they hold.

Usage example:

```cpp
// Get the canonical application audio state:
const Audio &audio = s.Audio; // Or just access the (read-only) `state` members directly

// Get the currently active gesture (collection of actions) from the global application context:
 const Gesture &ActiveGesture = c.ActiveGesture;
```
*/

extern const State &s;
extern Context c;

/**
 This is the main action-queue method.
 Providing `flush = true` will run all enqueued actions (including this one) and finalize any open gesture.
 This is useful for running multiple actions in a single frame, without grouping them into a single gesture.
*/
bool q(Action &&a, bool flush = false);

using FieldEntry = std::pair<const Field::Base &, Primitive>;
using FieldEntries = vector<FieldEntry>;

// Persistent (immutable) store setters
Store Set(const Field::Base &, const Primitive &, const Store &_store = store);
Store Set(const StoreEntries &, const Store &_store = store);
Store Set(const FieldEntries &, const Store &_store = store);

// Equivalent setters for a transient (mutable) store
void Set(const Field::Base &, const Primitive &, TransientStore &);
void Set(const StoreEntries &, TransientStore &);
void Set(const FieldEntries &, TransientStore &);

Patch CreatePatch(const Store &before, const Store &after, const StatePath &BasePath = RootPath);
