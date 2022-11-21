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
#include <variant>

#include "immer/map.hpp"
#include "immer/map_transient.hpp"
#include "nlohmann/json_fwd.hpp"
#include <range/v3/view/iota.hpp>
#include <range/v3/view/map.hpp>

#include "UI/UIContext.h"
#include "Helper/Sample.h"
#include "Helper/String.h"
#include "Helper/File.h"
#include "Helper/UI.h"

namespace FlowGrid {}
namespace fg = FlowGrid;

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

using ImS8 = signed char; // 8-bit signed integer
using ImU8 = unsigned char; // 8-bit unsigned integer
using ImS16 = signed short; // 16-bit signed integer
using ImU16 = unsigned short; // 16-bit unsigned integer
using ImS32 = signed int; // 32-bit signed integer == int
using ImU32 = unsigned int; // 32-bit unsigned integer (used to store packed colors & positions)
using ImS64 = signed long long; // 64-bit signed integer
using ImU64 = unsigned long long; // 64-bit unsigned integer

// Scalar data types, pointing to ImGui scalar types, with `{TypeName} = Im{TypeName}`.
using S8 = ImS8;
using U8 = ImU8;
using S16 = ImS16;
using U16 = ImU16;
using S32 = ImS32;
using U32 = ImU32;
using S64 = ImS64;
using U64 = ImU64;

using Count = U32;

// todo move to ImVec2ih, or make a new Vec2S16 type
constexpr U32 PackImVec2ih(const ImVec2ih &unpacked) { return (U32(unpacked.x) << 16) + U32(unpacked.y); }
constexpr ImVec2ih UnpackImVec2ih(const U32 packed) { return {S16(U32(packed) >> 16), S16(U32(packed) & 0xffff)}; }

using Primitive = std::variant<bool, U32, S32, float, string>;
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
using std::map;
//using std::set; todo capitalize all `set` global methods and uncomment

// E.g. '/foo/bar/baz' => 'baz'
inline string path_variable_name(const StatePath &path) { return path.filename(); }
inline string path_label(const StatePath &path) { return SnakeCaseToSentenceCase(path_variable_name(path)); }

// Split the string on '?'.
// If there is no '?' in the provided string, the first element will have the full input string and the second element will be an empty string.
// todo don't split on escaped '\?'
static std::pair<string, string> parse_help_text(const string &str) {
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

    // The `id` parameter is used as the path segment for this state member,
    // and optionally can contain a name and/or an info string.
    // Prefix a name segment with a '#', and an info segment with a '?'.
    // E.g. "TestMember#Test-member?A state member for testing things."
    // If no name segment is found, the name defaults to the path segment.
    StateMember(const StateMember *parent = nullptr, const string &id = "");
    StateMember(const StateMember *parent, const string &id, const Primitive &value);
    virtual ~StateMember();

    const StateMember *Parent;
    StatePath Path;
    string PathSegment, Name, Help;
    ID Id;
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

struct Linkable;

struct Bool : Base {
    Bool(const StateMember *parent, const string &identifier, bool value = false) : Base(parent, identifier, value) {}

    operator bool() const;

    bool Draw() const override;
    bool DrawMenu() const;

    std::set<Linkable *> Links;
private:
    void Toggle() const; // Used in draw methods.
};

struct Linkable {
    Linkable(Bool *linked) : Linked(linked) { if (Linked) Linked->Links.emplace(this); }
    ~Linkable() { if (Linked) Linked->Links.erase(this); }

    virtual void LinkValues() const = 0;

    Bool *Linked;
};

struct UInt : Base {
    UInt(const StateMember *parent, const string &id, U32 value = 0, U32 min = 0, U32 max = 100)
        : Base(parent, id, value), min(min), max(max) {}

    operator U32() const;
    operator bool() const { return (bool) (U32) *this; }

    bool operator==(int value) const { return int(*this) == value; }

    bool Draw() const override;

    U32 min, max;
};

struct Int : Base {
    Int(const StateMember *parent, const string &id, int value = 0, int min = 0, int max = 100)
        : Base(parent, id, value), min(min), max(max) {}

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
    Float(const StateMember *parent, const string &id, float value = 0, float min = 0, float max = 1, const char *fmt = nullptr)
        : Base(parent, id, value), min(min), max(max), fmt(fmt) {}

    operator float() const;

    bool Draw() const override;
    bool Draw(ImGuiSliderFlags flags) const;
    bool Draw(float v_speed, ImGuiSliderFlags flags) const;

    float min, max;
    const char *fmt;
};

struct String : Base {
    String(const StateMember *parent, const string &id, const string &value = "") : Base(parent, id, value) {}

    operator string() const;
    operator bool() const;

    bool operator==(const string &) const;

    bool Draw() const override;
    bool Draw(const vector<string> &options) const;
};

struct Enum : Base {
    Enum(const StateMember *parent, const string &id, vector<string> names, int value = 0)
        : Base(parent, id, value), names(std::move(names)) {}

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
            const auto &[name, help] = parse_help_text(name_and_help);
            Name = name;
            Help = help;
        }

        string Name, Help;
    };

    // All text after an optional '?' character for each name will be interpreted as an item help string.
    // E.g. `{"Foo?Does a thing", "Bar?Does a different thing", "Baz"}`
    Flags(const StateMember *parent, const string &id, vector<Item> items, int value = 0)
        : Base(parent, id, value), items(std::move(items)) {}

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

    Store set(Count index, const T &value, const Store &_store = store) const;
    Store set(const vector<T> &values, const Store &_store = store) const;
    Store set(const vector<std::pair<int, T>> &, const Store &_store = store) const;

    void set(Count index, const T &value, TransientStore &) const;
    void set(const vector<T> &values, TransientStore &) const;
    void set(const vector<std::pair<int, T>> &, TransientStore &) const;
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

    Store set(Count i, Count j, const T &value, const Store &_store = store) const;
    void set(Count i, Count j, const T &value, TransientStore &) const;
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

    Colors(const StateMember *parent, const string &path_segment, std::function<const char *(int)> get_color_name, const bool allow_auto = false)
        : Vector(parent, path_segment), AllowAuto(allow_auto), GetColorName(std::move(get_color_name)) {}

    static U32 ConvertFloat4ToU32(const ImVec4 &value) { return value == IMPLOT_AUTO_COL ? AutoColor : ImGui::ColorConvertFloat4ToU32(value); }
    static ImVec4 ConvertU32ToFloat4(const U32 value) { return value == AutoColor ? IMPLOT_AUTO_COL : ImGui::ColorConvertU32ToFloat4(value); }

    string GetName(Count index) const override { return GetColorName(int(index)); };
    bool Draw() const override;

    void set(const vector<ImVec4> &values, TransientStore &transient) const {
        Vector::set(values | transform([](const auto &value) { return ConvertFloat4ToU32(value); }) | to<vector>, transient);
    }
    void set(const vector<std::pair<int, ImVec4>> &entries, TransientStore &transient) const {
        Vector::set(entries | transform([](const auto &entry) { return std::pair(entry.first, ConvertFloat4ToU32(entry.second)); }) | to<vector>, transient);
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
    Window(const StateMember *parent, const string &id, bool visible = true);

    Bool Visible{this, "Visible", true};

    ImGuiWindow &FindImGuiWindow() const { return *ImGui::FindWindowByName(Name.c_str()); }
    void DrawWindow(ImGuiWindowFlags flags = ImGuiWindowFlags_None) const;
    void Dock(ID node_id) const;
    bool ToggleMenuItem() const;
    void SelectTab() const;
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
    Enum LabelMode{this, "LabelMode?The raw JSON state doesn't store keys for all items.\n"
                         "For example, the main `ui.style.colors` state is a list.\n\n"
                         "'Annotated' mode shows (highlighted) labels for such state items.\n"
                         "'Raw' mode shows the state exactly as it is in the raw JSON state.",
                   {"Annotated", "Raw"}, Annotated
    };
    Bool AutoSelect{this, "AutoSelect#Auto-Select?When auto-select is enabled, state changes automatically open.\n"
                          "The state viewer to the changed state node(s), closing all other state nodes.\n"
                          "State menu items can only be opened or closed manually if auto-select is disabled.", true};

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

    Enum Format{this, "Format", {"StateFormat", "ActionFormat"}, 1};
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
                Flags HoverFlags{
                    this, "HoverFlags?Hovering over a node in the graph will display the selected information",
                    {"ShowRect?Display the hovered node's bounding rectangle",
                     "ShowType?Display the hovered node's box type",
                     "ShowChannels?Display the hovered node's channel points and indices",
                     "ShowChildChannels?Display the channel points and indices for each of the hovered node's children"},
                    FaustDiagramHoverFlags_None
                };
            };

            DiagramSettings Settings{this, "Settings"};
        };

        struct FaustParams : Window {
            using Window::Window;
            void Draw() const override;
        };

        struct FaustLog : Window {
            using Window::Window;
            void Draw() const override;
        };

        FaustEditor Editor{this, "Editor#Faust editor"};
        FaustDiagram Diagram{this, "Diagram#Faust diagram"};
        FaustParams Params{this, "Params#Faust params"};
        FaustLog Log{this, "Log#Faust log"};

//        String Code{this, "Code", R"#(import("stdfaust.lib");
//pitchshifter = vgroup("Pitch Shifter", ef.transpose(
//    vslider("window (samples)", 1000, 50, 10000, 1),
//    vslider("xfade (samples)", 10, 1, 10000, 1),
//    vslider("shift (semitones)", 0, -24, +24, 0.1)
//  )
//);
//process = _ : pitchshifter;)#"};
//        String Code{this, "Code", R"#(import("stdfaust.lib");
//s = vslider("Signal[style:radio{'Noise':0;'Sawtooth':1}]",0,0,1,1);
//process = select2(s,no.noise,os.sawtooth(440));)#"};
//        String Code{this, "Code", R"(import("stdfaust.lib");
//process = ba.beat(240) : pm.djembe(60, 0.3, 0.4, 1) <: dm.freeverb_demo;)"};
//        String Code{this, "Code", R"(import("stdfaust.lib");
//process = _:fi.highpass(2,1000):_;)"};
//        String Code{this, "Code", R"(import("stdfaust.lib");
//ctFreq = hslider("cutoffFrequency",500,50,10000,0.01);
//q = hslider("q",5,1,30,0.1);
//gain = hslider("gain",1,0,1,0.01);
//process = no:noise : fi.resonlp(ctFreq,q,gain);)"};

// Based on Faust::UITester.dsp
        String Code{this, "Code", R"#(import("stdfaust.lib");
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
        String Error{this, "Error"};
    };

    void UpdateProcess() const;
    const String &GetDeviceId(IO io) const { return io == IO_In ? InDeviceId : OutDeviceId; }

    Bool Running{this, format("Running?Disabling ends the {} process.\nEnabling will start the process up again.", lowercase(Name)), true};
    Bool FaustRunning{this, "FaustRunning?Disabling skips Faust computation when computing audio output.", true};
    Bool Muted{this, "Muted?Enabling sets all audio output to zero.\nAll audio computation will still be performed, so this setting does not affect CPU load.", true};
    AudioBackend Backend = none;
    String InDeviceId{this, "InDeviceId#In device ID"};
    String OutDeviceId{this, "OutDeviceId#Out device ID"};
    Int InSampleRate{this, "InSampleRate"};
    Int OutSampleRate{this, "OutSampleRate"};
    Enum InFormat{this, "InFormat", {"Invalid", "Float64", "Float32", "Short32", "Short16"}, IoFormat_Invalid};
    Enum OutFormat{this, "OutFormat", {"Invalid", "Float64", "Float32", "Short32", "Short16"}, IoFormat_Invalid};
    Float OutDeviceVolume{this, "OutDeviceVolume", 1.0};
    Bool MonitorInput{this, "MonitorInput?Enabling adds the audio input stream directly to the audio output."};

    FaustState Faust{this, "Faust"};
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

struct Vec2 : UIStateMember, Linkable {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Vec2(const StateMember *parent, const string &id, const ImVec2 &value = {0, 0}, float min = 0, float max = 1,
         const char *fmt = nullptr, Bool *link_values = nullptr)
        : UIStateMember(parent, id), Linkable(link_values),
          X(this, "X", value.x, min, max), Y(this, "Y", value.y, min, max),
          min(min), max(max), fmt(fmt) {}

    operator ImVec2() const { return {X, Y}; }

    void Draw() const override;
    bool Draw(ImGuiSliderFlags flags) const;
    void LinkValues() const override;

    Float X, Y;

    float min, max; // todo don't need these anymore, derive from X/Y
    const char *fmt;
};

struct Vec2Int : UIStateMember {
    Vec2Int(const StateMember *parent, const string &id, const ImVec2ih &value = {0, 0}, int min = 0, int max = 1)
        : UIStateMember(parent, id),
          X(this, "X", value.x, min, max), Y(this, "Y", value.y, min, max),
          min(min), max(max) {}

    operator ImVec2ih() const { return {X, Y}; }
    operator ImVec2() const { return {float(int(X)), float(int(Y))}; }

    void Draw() const override;

    Int X, Y;
    int min, max;
};

struct Style : Window {
    using Window::Window;

    void Draw() const override;

    struct FlowGridStyle : UIStateMember {
        FlowGridStyle(const StateMember *parent, const string &id);
        void Draw() const override;

        Float FlashDurationSec{this, "FlashDurationSec", 0.6, 0, 5};

        UInt DiagramFoldComplexity{
            this, "DiagramFoldComplexity?Number of boxes within a diagram before folding into a sub-diagram.\n"
                  "Setting to zero disables folding altogether, for a fully-expanded diagram.", 3, 0, 20};
        Bool DiagramScaleLinked{this, "DiagramScaleLinked?Link X/Y", true}; // Link X/Y scale sliders, forcing them to the same value.
        Bool DiagramScaleFill{this, "DiagramScaleFill?Scale to fill the window.\nEnabling this setting deactivates other diagram scale settings."};
        Vec2 DiagramScale{this, "DiagramScale", {1, 1}, 0.1, 10, nullptr, &DiagramScaleLinked};
        Enum DiagramDirection{this, "DiagramDirection", {"Left", "Right"}, ImGuiDir_Right};
        Bool DiagramRouteFrame{this, "DiagramRouteFrame"};
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
            this, "ParamsWidthSizingPolicy?StretchFlexibleOnly: If a table contains only fixed-width items, it won't stretch to fill available width.\n"
                  "StretchToFill: If a table contains only fixed-width items, allow columns to stretch to fill available width.\n"
                  "Balanced: All param types are given flexible-width, weighted by their minimum width. (Looks more balanced, but less expansion room for wide items).",
            {"StretchToFill", "StretchFlexibleOnly", "Balanced"}, ParamsWidthSizingPolicy_StretchFlexibleOnly};

        Colors Colors{this, "Colors", GetColorName};

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
        ImGuiStyle(const StateMember *parent, const string &id);

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
        Vec2 WindowPadding{this, "WindowPadding", {8, 8}, 0, 20, "%.0f"};
        Vec2 FramePadding{this, "FramePadding", {4, 3}, 0, 20, "%.0f"};
        Vec2 CellPadding{this, "CellPadding", {4, 2}, 0, 20, "%.0f"};
        Vec2 ItemSpacing{this, "ItemSpacing", {8, 4}, 0, 20, "%.0f"};
        Vec2 ItemInnerSpacing{this, "ItemInnerSpacing", {4, 4}, 0, 20, "%.0f"};
        Vec2 TouchExtraPadding{this, "TouchExtraPadding", {0, 0}, 0, 10, "%.0f"};
        Float IndentSpacing{this, "IndentSpacing", 21, 0, 30, "%.0f"};
        Float ScrollbarSize{this, "ScrollbarSize", 14, 1, 20, "%.0f"};
        Float GrabMinSize{this, "GrabMinSize", 12, 1, 20, "%.0f"};

        // Borders
        Float WindowBorderSize{this, "WindowBorderSize", 1, 0, 1, "%.0f"};
        Float ChildBorderSize{this, "ChildBorderSize", 1, 0, 1, "%.0f"};
        Float FrameBorderSize{this, "FrameBorderSize", 0, 0, 1, "%.0f"};
        Float PopupBorderSize{this, "PopupBorderSize", 1, 0, 1, "%.0f"};
        Float TabBorderSize{this, "TabBorderSize", 0, 0, 1, "%.0f"};

        // Rounding
        Float WindowRounding{this, "WindowRounding", 0, 0, 12, "%.0f"};
        Float ChildRounding{this, "ChildRounding", 0, 0, 12, "%.0f"};
        Float FrameRounding{this, "FrameRounding", 0, 0, 12, "%.0f"};
        Float PopupRounding{this, "PopupRounding", 0, 0, 12, "%.0f"};
        Float ScrollbarRounding{this, "ScrollbarRounding", 9, 0, 12, "%.0f"};
        Float GrabRounding{this, "GrabRounding", 0, 0, 12, "%.0f"};
        Float LogSliderDeadzone{this, "LogSliderDeadzone", 4, 0, 12, "%.0f"};
        Float TabRounding{this, "TabRounding", 4, 0, 12, "%.0f"};

        // Alignment
        Vec2 WindowTitleAlign{this, "WindowTitleAlign", {0, 0.5}, 0, 1, "%.2f"};
        Enum WindowMenuButtonPosition{this, "WindowMenuButtonPosition", {"Left", "Right"}, ImGuiDir_Left};
        Enum ColorButtonPosition{this, "ColorButtonPosition", {"Left", "Right"}, ImGuiDir_Right};
        Vec2 ButtonTextAlign{this, "ButtonTextAlign?Alignment applies when a button is larger than its text content.", {0.5, 0.5}, 0, 1, "%.2f"};
        Vec2 SelectableTextAlign{this, "SelectableTextAlign?Alignment applies when a selectable is larger than its text content.", {0, 0}, 0, 1, "%.2f"};

        // Safe area padding
        Vec2 DisplaySafeAreaPadding{this, "DisplaySafeAreaPadding?Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured).", {3, 3}, 0, 30, "%.0f"};

        // Rendering
        Bool AntiAliasedLines{this, "AntiAliasedLines#Anti-aliased lines?When disabling anti-aliasing lines, you'll probably want to disable borders in your style as well.", true};
        Bool AntiAliasedLinesUseTex{this, "AntiAliasedLinesUseTex#Anti-aliased lines use texture?Faster lines using texture data. Require backend to render with bilinear filtering (not point/nearest filtering).", true};
        Bool AntiAliasedFill{this, "AntiAliasedFill#Anti-aliased fill", true};
        Float CurveTessellationTol{this, "CurveTessellationTol#Curve tesselation tolerance", 1.25, 0.1, 10, "%.2f"};
        Float CircleTessellationMaxError{this, "CircleTessellationMaxError", 0.3, 0.1, 5, "%.2f"};
        Float Alpha{this, "Alpha", 1, 0.2, 1, "%.2f"}; // Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets).
        Float DisabledAlpha{this, "DisabledAlpha?Additional alpha multiplier for disabled items (multiply over current value of Alpha).", 0.6, 0, 1, "%.2f"};

        // Fonts
        Int FontIndex{this, "FontIndex"};
        Float FontScale{this, "FontScale?Global font scale (low-quality!)", 1, 0.3, 2, "%.2f"}; // todo add flags option and use `ImGuiSliderFlags_AlwaysClamp` here

        // Not editable todo delete?
        Float TabMinWidthForCloseButton{this, "TabMinWidthForCloseButton", 0};
        Vec2 DisplayWindowPadding{this, "DisplayWindowPadding", {19, 19}};
        Vec2 WindowMinSize{this, "WindowMinSize", {32, 32}};
        Float MouseCursorScale{this, "MouseCursorScale", 1};
        Float ColumnsMinSpacing{this, "ColumnsMinSpacing", 6};

        Colors Colors{this, "Colors", ImGui::GetStyleColorName};
    };
    struct ImPlotStyle : UIStateMember {
        ImPlotStyle(const StateMember *parent, const string &id);
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
        Float LineWeight{this, "LineWeight", 1, 0, 5, "%.1f"};
        Float MarkerSize{this, "MarkerSize", 4, 2, 10, "%.1f"};
        Float MarkerWeight{this, "MarkerWeight", 1, 0, 5, "%.1f"};
        Float FillAlpha{this, "FillAlpha", 1, 0, 1, "%.2f"};
        Float ErrorBarSize{this, "ErrorBarSize", 5, 0, 10, "%.1f"};
        Float ErrorBarWeight{this, "ErrorBarWeight", 1.5, 0, 5, "%.1f"};
        Float DigitalBitHeight{this, "DigitalBitHeight", 8, 0, 20, "%.1f"};
        Float DigitalBitGap{this, "DigitalBitGap", 4, 0, 20, "%.1f"};

        // Plot styling
        Float PlotBorderSize{this, "PlotBorderSize", 1, 0, 2, "%.0f"};
        Float MinorAlpha{this, "MinorAlpha", 0.25, 1, 0, "%.2f"};
        Vec2 MajorTickLen{this, "MajorTickLen", {10, 10}, 0, 20, "%.0f"};
        Vec2 MinorTickLen{this, "MinorTickLen", {5, 5}, 0, 20, "%.0f"};
        Vec2 MajorTickSize{this, "MajorTickSize", {1, 1}, 0, 2, "%.1f"};
        Vec2 MinorTickSize{this, "MinorTickSize", {1, 1}, 0, 2, "%.1f"};
        Vec2 MajorGridSize{this, "MajorGridSize", {1, 1}, 0, 2, "%.1f"};
        Vec2 MinorGridSize{this, "MinorGridSize", {1, 1}, 0, 2, "%.1f"};
        Vec2 PlotDefaultSize{this, "PlotDefaultSize", {400, 300}, 0, 1000, "%.0f"};
        Vec2 PlotMinSize{this, "PlotMinSize", {200, 150}, 0, 300, "%.0f"};

        // Plot padding
        Vec2 PlotPadding{this, "PlotPadding", {10, 10}, 0, 20, "%.0f"};
        Vec2 LabelPadding{this, "LabelPadding", {5, 5}, 0, 20, "%.0f"};
        Vec2 LegendPadding{this, "LegendPadding", {10, 10}, 0, 20, "%.0f"};
        Vec2 LegendInnerPadding{this, "LegendInnerPadding", {5, 5}, 0, 10, "%.0f"};
        Vec2 LegendSpacing{this, "LegendSpacing", {5, 0}, 0, 5, "%.0f"};
        Vec2 MousePosPadding{this, "MousePosPadding", {10, 10}, 0, 20, "%.0f"};
        Vec2 AnnotationPadding{this, "AnnotationPadding", {2, 2}, 0, 5, "%.0f"};
        Vec2 FitPadding{this, "FitPadding", {0, 0}, 0, 0.2, "%.2f"};

        Colors Colors{this, "Colors", ImPlot::GetStyleColorName, true};
        Bool UseLocalTime{this, "UseLocalTime"};
        Bool UseISO8601{this, "UseISO8601"};
        Bool Use24HourClock{this, "Use24HourClock"};

        // Not editable todo delete?
        Int Marker{this, "Marker", ImPlotMarker_None};
    };

    ImGuiStyle ImGui{this, "ImGui?Configure style for base UI"};
    ImPlotStyle ImPlot{this, "ImPlot?Configure style for plots"};
    FlowGridStyle FlowGrid{this, "FlowGrid?Configure application-specific style"};
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

    Vector<ID> NodeId{this, "NodeId"};
    Vector<ID> ParentNodeId{this, "ParentNodeId"};
    Vector<ID> ParentWindowId{this, "ParentWindowId"};
    Vector<ID> SelectedTabId{this, "SelectedTabId"};
    Vector<int> SplitAxis{this, "SplitAxis"};
    Vector<int> Depth{this, "Depth"};
    Vector<int> Flags{this, "Flags"};
    Vector<U32> Pos{this, "Pos"}; // Packed ImVec2ih
    Vector<U32> Size{this, "Size"}; // Packed ImVec2ih
    Vector<U32> SizeRef{this, "SizeRef"}; // Packed ImVec2ih
};

struct WindowSettings : StateMember {
    using StateMember::StateMember;
    void Set(ImChunkStream<ImGuiWindowSettings> &, TransientStore &store) const;
    void Apply(ImGuiContext *) const;

    Vector<ImGuiID> ID{this, "ID"};
    Vector<ImGuiID> ClassId{this, "ClassId"};
    Vector<ImGuiID> ViewportId{this, "ViewportId"};
    Vector<ImGuiID> DockId{this, "DockId"};
    Vector<int> DockOrder{this, "DockOrder"};
    Vector<U32> Pos{this, "Pos"}; // Packed ImVec2ih
    Vector<U32> Size{this, "Size"}; // Packed ImVec2ih
    Vector<U32> ViewportPos{this, "ViewportPos"}; // Packed ImVec2ih
    Vector<bool> Collapsed{this, "Collapsed"};
};

struct TableColumnSettings : StateMember {
    using StateMember::StateMember;

    // [table_index][column_index]
    Vector2D<float> WidthOrWeight{this, "WidthOrWeight"};
    Vector2D<ID> UserID{this, "UserID"};
    Vector2D<int> Index{this, "Index"};
    Vector2D<int> DisplayOrder{this, "DisplayOrder"};
    Vector2D<int> SortOrder{this, "SortOrder"};
    Vector2D<int> SortDirection{this, "SortDirection"};
    Vector2D<bool> IsEnabled{this, "IsEnabled"}; // "Visible" in ini file
    Vector2D<bool> IsStretch{this, "IsStretch"};
};

struct TableSettings : StateMember {
    using StateMember::StateMember;
    void Set(ImChunkStream<ImGuiTableSettings> &, TransientStore &store) const;
    void Apply(ImGuiContext *) const;

    Vector<ImGuiID> ID{this, "ID"};
    Vector<int> SaveFlags{this, "SaveFlags"};
    Vector<float> RefScale{this, "RefScale"};
    Vector<Count> ColumnsCount{this, "ColumnsCount"};
    Vector<Count> ColumnsCountMax{this, "ColumnsCountMax"};
    Vector<bool> WantApply{this, "WantApply"};
    TableColumnSettings Columns{this, "Columns"};
};

struct ImGuiSettings : StateMember {
    using StateMember::StateMember;
    Store set(ImGuiContext *ctx) const;
    // Inverse of above constructor. `imgui_context.settings = this`
    // Should behave just like `ImGui::LoadIniSettingsFromMemory`, but using the structured `...Settings` members
    //  in this struct instead of the serialized .ini text format.
    void Apply(ImGuiContext *ctx) const;

    DockNodeSettings Nodes{this, "Nodes"};
    WindowSettings Windows{this, "Windows"};
    TableSettings Tables{this, "Tables"};
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
constexpr int FileDialogFlags_Modal = 1 << 27; // Copied from `ImGuiFileDialog` source with a different name to avoid redefinition. Brittle but we can avoid an include this way.

struct FileDialogData {
    string title = "Choose file", filters, file_path = ".", default_file_name;
    bool save_mode = false;
    int max_num_selections = 1;
    ImGuiFileDialogFlags flags = 0;
};

struct FileDialog : Window {
    FileDialog(const StateMember *parent, const string &id, const bool visible = false) : Window(parent, id, visible) {}
    void set(const FileDialogData &data, TransientStore &) const;
    void Draw() const override;

    Bool SaveMode{this, "SaveMode"}; // The same file dialog instance is used for both saving & opening files.
    Int MaxNumSelections{this, "MaxNumSelections", 1};
//        ImGuiFileDialogFlags Flags;
    Int Flags{this, "Flags", FileDialogFlags_Modal};
    String Title{this, "Title", "Choose file"};
    String Filters{this, "Filters"};
    String FilePath{this, "FilePath", "."};
    String DefaultFileName{this, "DefaultFileName"};
};

enum PatchOpType {
    Add,
    Remove,
    Replace,
};
struct PatchOp {
    PatchOpType Op{};
    std::optional<Primitive> Value{}; // Present for add/replace
    std::optional<Primitive> Old{}; // Present for remove/replace
};
using PatchOps = map<StatePath, PatchOp>;

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
string to_string(PatchOpType);

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
    {id<save_current_project>, "cmd+s"},
    {id<open_default_project>, "shift+cmd+o"},
    {id<save_default_project>, "shift+cmd+s"},
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

        Bool Running{this, format("Running?Disabling ends the {} process.\nEnabling will start the process up again.", lowercase(Name)), true};
    };

    ImGuiSettings ImGuiSettings{this, "ImGuiSettings#ImGui settings"};
    Style Style{this, "Style"};
    ApplicationSettings ApplicationSettings{this, "ApplicationSettings"};
    Audio Audio{this, "Audio"};
    UIProcess UiProcess{this, "UiProcess"};
    FileDialog FileDialog{this, "FileDialog"};
    Info Info{this, "Info"};

    Demo Demo{this, "Demo"};
    Metrics Metrics{this, "Metrics"};
    StackTool StackTool{this, "StackTool"};
    DebugLog DebugLog{this, "DebugLog"};

    StateViewer StateViewer{this, "StateViewer"};
    StateMemoryEditor StateMemoryEditor{this, "StateMemoryEditor"};
    StatePathUpdateFrequency PathUpdateFrequency{this, "PathUpdateFrequency#State path update frequency"};
    ProjectPreview ProjectPreview{this, "ProjectPreview"};
};

//-----------------------------------------------------------------------------
// [SECTION] Main application `Context`
//-----------------------------------------------------------------------------

static const map<ProjectFormat, string> ExtensionForProjectFormat{{StateFormat, ".fls"}, {ActionFormat, ".fla"}};
static const auto ProjectFormatForExtension = ExtensionForProjectFormat |
    transform([](const auto &pair) { return std::pair(pair.second, pair.first); }) | to<map>();
static const auto AllProjectExtensions = views::keys(ProjectFormatForExtension) | to<std::set>;
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

private:
    void ApplyAction(const ProjectAction &);

    void SetCurrentProjectPath(const fs::path &);
    bool WritePreferences() const;

    std::optional<fs::path> CurrentProjectPath;
};

//-----------------------------------------------------------------------------
// [SECTION] Widgets
//-----------------------------------------------------------------------------

namespace FlowGrid {
void HelpMarker(const char *help);

void MenuItem(const EmptyAction &); // For actions with no data members.

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

using MemberEntry = std::pair<const StateMember &, Primitive>;
using MemberEntries = vector<MemberEntry>;

// Persistent (immutable) store setters
Store set(const StateMember &member, const Primitive &value, const Store &_store = store);
Store set(const StoreEntries &, const Store &_store = store);
Store set(const MemberEntries &, const Store &_store = store);

// Equivalent setters for a transient (mutable) store
void set(const StateMember &, const Primitive &, TransientStore &);
void set(const StoreEntries &, TransientStore &);
void set(const MemberEntries &, TransientStore &);

Patch CreatePatch(const Store &before, const Store &after, const StatePath &BasePath = RootPath);
