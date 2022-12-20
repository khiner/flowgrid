#pragma once

/**
 * The main `State` instance fully describes the application at any point in time.
 *
 * The entire codebase has read-only access to the immutable, single source-of-truth application `const State &s` instance,
 * which also provides an immutable `Update(const Action &, TransientState &) const` method, and a `Draw() const` method.
 */
#include <iostream>
#include <list>
#include <queue>
#include <set>

#include "immer/map.hpp"
#include "immer/map_transient.hpp"
#include "nlohmann/json_fwd.hpp"
#include <range/v3/view/iota.hpp>
#include <range/v3/view/map.hpp>

#include "Actions.h"
#include "Helper/Sample.h"
#include "UI/Style.h"
#include "UI/UI.h"

#include "imgui_internal.h"

namespace FlowGrid {}
namespace fg = FlowGrid;

using namespace StringHelper;

using fmt::format, fmt::to_string;
using namespace nlohmann;
using std::cout, std::cerr;
using std::set;
using std::min, std::max;
using std::nullopt, std::unique_ptr, std::make_unique;
using Store = immer::map<StatePath, Primitive, StatePathHash>;
using TransientStore = immer::map_transient<StatePath, Primitive, StatePathHash>;

extern const Store &AppStore; // Read-only global for full, read-only canonical application state.

using action::ActionMoment, action::Gesture, action::Gestures, action::StateActionMoment;

// todo move to ImVec2ih, or make a new Vec2S16 type
constexpr U32 PackImVec2ih(const ImVec2ih &unpacked) { return (U32(unpacked.x) << 16) + U32(unpacked.y); }
constexpr ImVec2ih UnpackImVec2ih(const U32 packed) { return {S16(U32(packed) >> 16), S16(U32(packed) & 0xffff)}; }

constexpr bool operator==(const ImVec4 &lhs, const ImVec4 &rhs) { return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w; }

// E.g. '/foo/bar/baz' => 'baz'
inline string PathVariableName(const StatePath &path) { return path.filename(); }
inline string PathLabel(const StatePath &path) { return SnakeCaseToSentenceCase(PathVariableName(path)); }

// Split the string on '?'.
// If there is no '?' in the provided string, the first element will have the full input string and the second element will be an empty string.
// todo don't split on escaped '\?'
static pair<string_view, string_view> ParseHelpText(string_view str) {
    const auto help_split = str.find_first_of('?');
    const bool found = help_split != string::npos;
    return {found ? str.substr(0, help_split) : str, found ? str.substr(help_split + 1) : ""};
}

struct Preferences {
    std::list<fs::path> RecentlyOpenedPaths{};
};

struct StateMember {
    static map<ID, StateMember *> WithId; // Allows for access of any state member by ImGui ID

    StateMember(StateMember *parent = nullptr, string_view path_segment = "", string_view name_help = "");
    virtual ~StateMember();

    const StateMember *Parent;
    StatePath Path;
    string PathSegment, Name, Help, ImGuiLabel;
    ID Id;
    vector<StateMember *> Children{};

protected:
    // Helper to display a (?) mark which shows a tooltip when hovered. Similar to the one in `imgui_demo.cpp`.
    void HelpMarker(bool after = true) const;
};

/**
Convenience macros for compactly defining `StateMember` types and their properties.

todo These will very likely be defined in a separate language once the API settles down.
  If we could hot-reload and only recompile the needed bits without restarting the app, it would accelerate development A TON.
  (Long compile times, although they aren't nearly as bad as [my previous attempt](https://github.com/khiner/flowgrid_old),
  are still the biggest drain on this project.)

Macros:

All macros end in semicolons already, so there's no strict need to suffix their usage with a semicolon.
However, all macro calls in FlowGrid are treated like regular function calls, appending a semicolon.
todo If we stick with this, add a `static_assert(true, "")` to the end of all macro definitions.
https://stackoverflow.com/questions/35530850/how-to-require-a-semicolon-after-a-macro
todo Try out replacing semicolon separators by e.g. commas.

* Properties
  - `Prop` adds a new property `this`.
  Define a property of this type during construction at class scope to add a child member variable.
    - Assumes it's being called within a `PropType` class scope during construction.
    `PropType`, with variable name `PropName`, constructing the state member with `this` as a parent, and store path-segment `"{PropName}"`.
    (string with value the same as the variable name).
    - `Prop_` is the same as `Prop`, but supports overriding the displayed name & adding help text in the third arg.
    - Arguments
      1) `PropType`: Any type deriving from `StateMember`.
      2) `PropName` (use PascalCase) is used for:
        - The ID of the property, relative to its parent (`this` during the macro's execution).
        - The name of the instance variable added to `this` (again, defined like any other instance variable in a `StateMember`).
        - The label displayed in the UI is derived from the prop's (PascalCase) `PropName` property-id/path-segment (the second arg).
      3) `NameHelp`
        - A string with format "Label string?Help string".
        - Optional, available with a `_` suffix.
        - Overrides the label displayed in the UI for this property.
        - Anything after a '?' is interpretted as a help string
          - E.g. `Prop(Bool, TestAThing, "Test-a-thing?A state member for testing things")` overrides the default "TestAThing" PascaleCase label with a hyphenation.
            - todo "Sentence case" label
          - Or, provide nothing before the '?' to add a help string without overriding the default `PropName`-derived label.
            - E.g. "?A state member for testing things."
* Member types
  - `Member` defines a plain old state type.
  - `UIMember` defines a drawable state type.
    - `UIMember_` is the same as `UIMember`, but adds a custom constructor implementation (with the same arguments).
  - `WindowMember` defines a drawable state type whose contents are rendered to a window.
    - `WindowMember_` is the same as `WindowMember`, but allows passing either:
      - a `bool` to override the default `true` visibility, or
      - a `Menu` to define the window's menu.
    - `TabsWindow` is a `WindowMember` that renders all its props as tabs. (except the `Visible` boolean member coming from `WindowMember`).
  - todo Refactor docking behavior out of `WindowMember` into a new `DockMember` type.

**/

#define Prop(PropType, PropName, ...) PropType PropName{this, (#PropName), "", __VA_ARGS__};
#define Prop_(PropType, PropName, NameHelp, ...) PropType PropName{this, (#PropName), (NameHelp), __VA_ARGS__};

#define Member(MemberName, ...)         \
    struct MemberName : StateMember {   \
        using StateMember::StateMember; \
        __VA_ARGS__;                    \
    };

struct Drawable {
    virtual void Draw() const; // Wraps around the internal `Render` function.

protected:
    virtual void Render() const = 0;
};

struct MenuItemDrawable {
    virtual void MenuItem() const = 0;
};

struct Menu : Drawable {
    using Item = std::variant<
        const Menu,
        const std::reference_wrapper<MenuItemDrawable>,
        const EmptyAction>;

    Menu(string_view label, const vector<const Item> items) : Label(label), Items(std::move(items)) {}
    explicit Menu(const vector<const Item> items) : Menu("", std::move(items)) {}
    Menu(const vector<const Item> items, const bool is_main) : Label(""), Items(std::move(items)), IsMain(is_main) {}

    const string Label; // If no label is provided, this is rendered as a top-level window menu bar.
    const vector<const Item> Items;
    const bool IsMain{false};

protected:
    void Render() const override;
};

struct UIStateMember : StateMember, Drawable {
    using StateMember::StateMember;
};

#define UIMember(MemberName, ...)           \
    struct MemberName : UIStateMember {     \
        using UIStateMember::UIStateMember; \
        __VA_ARGS__;                        \
                                            \
    protected:                              \
        void Render() const override;       \
    };

#define UIMember_(MemberName, ...)                                                             \
    struct MemberName : UIStateMember {                                                        \
        MemberName(StateMember *parent, string_view path_segment, string_view name_help = ""); \
        __VA_ARGS__;                                                                           \
                                                                                               \
    protected:                                                                                 \
        void Render() const override;                                                          \
    };

#define WindowMember(MemberName, ...) \
    struct MemberName : Window {      \
        using Window::Window;         \
        __VA_ARGS__;                  \
                                      \
    protected:                        \
        void Render() const override; \
    };

#define WindowMember_(MemberName, VisibleOrMenu, ...)                                         \
    struct MemberName : Window {                                                              \
        MemberName(StateMember *parent, string_view path_segment, string_view name_help = "") \
            : Window(parent, path_segment, name_help, (VisibleOrMenu)) {}                     \
        __VA_ARGS__;                                                                          \
                                                                                              \
    protected:                                                                                \
        void Render() const override;                                                         \
    };

// A `Field` is a drawable state-member that wraps around a primitive type.
namespace Field {

struct Base : UIStateMember {
    Base(StateMember *parent, string_view path_segment, string_view name_help, const Primitive &value);

    Primitive Get() const; // Returns the value in the main application state store.
    Primitive GetInitial() const; // Returns the value in the initialization state store.
};

struct Bool : Base, MenuItemDrawable {
    Bool(StateMember *parent, string_view path_segment, string_view name_help, bool value = false)
        : Base(parent, path_segment, name_help, value) {}

    operator bool() const;
    bool CheckedDraw() const; // Unlike `Draw`, this returns `true` if the value was toggled during the draw.
    void MenuItem() const override;

protected:
    void Render() const override;

private:
    void Toggle() const; // Used in draw methods.
};

struct UInt : Base {
    UInt(StateMember *parent, string_view path_segment, string_view name_help, U32 value = 0, U32 min = 0, U32 max = 100)
        : Base(parent, path_segment, name_help, value), min(min), max(max) {}

    operator U32() const;
    operator bool() const { return bool(U32(*this)); }
    bool operator==(int value) const { return int(*this) == value; }

    U32 min, max;

protected:
    void Render() const override;
};

struct Int : Base {
    Int(StateMember *parent, string_view path_segment, string_view name_help, int value = 0, int min = 0, int max = 100)
        : Base(parent, path_segment, name_help, value), min(min), max(max) {}

    operator int() const;
    operator bool() const { return bool(int(*this)); }
    operator short() const { return short(int(*this)); }
    operator char() const { return char(int(*this)); }
    operator S8() const { return S8(int(*this)); }
    bool operator==(int value) const { return int(*this) == value; }

    void Render(const vector<int> &options) const;

    int min, max;

protected:
    void Render() const override;
};

struct Float : Base {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Float(StateMember *parent, string_view path_segment, string_view name_help, float value = 0, float min = 0, float max = 1, const char *fmt = nullptr, ImGuiSliderFlags flags = ImGuiSliderFlags_None, float drag_speed = 0)
        : Base(parent, path_segment, name_help, value), Min(min), Max(max), DragSpeed(drag_speed), Format(fmt), Flags(flags) {}

    operator float() const;

    const float Min, Max, DragSpeed; // If `DragSpeed` is non-zero, this is rendered as an `ImGui::DragFloat`.
    const char *Format;
    const ImGuiSliderFlags Flags;

protected:
    void Render() const override;
};

struct String : Base {
    String(StateMember *parent, string_view path_segment, string_view name_help, string_view value = "")
        : Base(parent, path_segment, name_help, string(value)) {}

    operator string() const;
    operator bool() const;
    bool operator==(const string &) const;

    void Render(const vector<string> &options) const;

protected:
    void Render() const override;
};

struct Enum : Base, MenuItemDrawable {
    Enum(StateMember *parent, string_view path_segment, string_view name_help, vector<string> names, int value = 0)
        : Base(parent, path_segment, name_help, value), Names(std::move(names)) {}

    operator int() const;

    void Render(const vector<int> &options) const;
    void MenuItem() const override;

    const vector<string> Names;

protected:
    void Render() const override;
};

// todo in state viewer, make `Annotated` label mode expand out each integer flag into a string list
struct Flags : Base, MenuItemDrawable {
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
    Flags(StateMember *parent, string_view path_segment, string_view name_help, vector<Item> items, int value = 0)
        : Base(parent, path_segment, name_help, value), Items(std::move(items)) {}

    operator int() const;

    void MenuItem() const override;

    const vector<Item> Items;

protected:
    void Render() const override;
};
} // namespace Field

using FieldEntry = pair<const Field::Base &, Primitive>;
using FieldEntries = vector<FieldEntry>;

using namespace Field;

template<typename T>
struct Vector : StateMember {
    using StateMember::StateMember;

    virtual string GetName(Count index) const { return to_string(index); };

    T operator[](Count index) const;
    Count size(const Store &store = AppStore) const;

    Store Set(Count index, const T &value, const Store &store = AppStore) const;
    Store Set(const vector<T> &values, const Store &store = AppStore) const;
    Store Set(const vector<pair<int, T>> &, const Store &store = AppStore) const;

    void Set(Count index, const T &value, TransientStore &) const;
    void Set(const vector<T> &values, TransientStore &) const;
    void Set(const vector<pair<int, T>> &, TransientStore &) const;

    void truncate(Count length, TransientStore &) const; // Delete every element after index `length - 1`.
};

// Really a vector of vectors. Inner vectors need not have the same length.
template<typename T>
struct Vector2D : StateMember {
    using StateMember::StateMember;

    T at(Count i, Count j, const Store &store = AppStore) const;
    Count size(const TransientStore &) const; // Number of outer vectors

    Store Set(Count i, Count j, const T &value, const Store &store = AppStore) const;
    void Set(Count i, Count j, const T &value, TransientStore &) const;
    void truncate(Count length, TransientStore &) const; // Delete every outer vector after index `length - 1`.
    void truncate(Count i, Count length, TransientStore &) const; // Delete every element after index `length - 1` in inner vector `i`.
};

struct Colors : Vector<U32> {
    // An arbitrary transparent color is used to mark colors as "auto".
    // Using a the unique bit pattern `010101` for the RGB components so as not to confuse it with black/white-transparent.
    // Similar to ImPlot's usage of [`IMPLOT_AUTO_COL = ImVec4(0,0,0,-1)`](https://github.com/epezent/implot/blob/master/implot.h#L67),
    // but not using a value so it can be represented in a U32.
    static constexpr U32 AutoColor = 0X00010101;

    Colors(StateMember *parent, string_view path_segment, string_view name_help, std::function<const char *(int)> get_color_name, const bool allow_auto = false)
        : Vector(parent, path_segment, name_help), AllowAuto(allow_auto), GetColorName(std::move(get_color_name)) {}

    static U32 ConvertFloat4ToU32(const ImVec4 &value) { return value == IMPLOT_AUTO_COL ? AutoColor : ImGui::ColorConvertFloat4ToU32(value); }
    static ImVec4 ConvertU32ToFloat4(const U32 value) { return value == AutoColor ? IMPLOT_AUTO_COL : ImGui::ColorConvertU32ToFloat4(value); }

    string GetName(Count index) const override { return GetColorName(int(index)); };
    void Draw() const;

    void Set(const vector<ImVec4> &values, TransientStore &transient) const {
        Vector::Set(values | transform([](const auto &value) { return ConvertFloat4ToU32(value); }) | to<vector>, transient);
    }
    void Set(const vector<pair<int, ImVec4>> &entries, TransientStore &transient) const {
        Vector::Set(entries | transform([](const auto &entry) { return pair(entry.first, ConvertFloat4ToU32(entry.second)); }) | to<vector>, transient);
    }

private:
    bool AllowAuto;
    const std::function<const char *(int)> GetColorName;
};

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

ImGuiTableFlags TableFlagsToImgui(TableFlags);

struct Window : UIStateMember, MenuItemDrawable {
    using UIStateMember::UIStateMember;
    Window(StateMember *parent, string_view path_segment, string_view name_help, bool visible);
    Window(StateMember *parent, string_view path_segment, string_view name_help, ImGuiWindowFlags flags);
    Window(StateMember *parent, string_view path_segment, string_view name_help, Menu menu);

    ImGuiWindow &FindImGuiWindow() const { return *ImGui::FindWindowByName(ImGuiLabel.c_str()); }
    void Draw() const override;
    void MenuItem() const override; // Rendering a window as a menu item shows a window visibility toggle, with the window name as the label.
    void Dock(ID node_id) const;
    void SelectTab() const; // If this window is tabbed, select it.

    Prop(Bool, Visible, true);

    const Menu WindowMenu{{}};
    const ImGuiWindowFlags WindowFlags{ImGuiWindowFlags_None};
};

// When we define a window member type without adding properties, we're defining a new way to arrange and draw the children of the window.
// The controct we're signing up for is to implement `void TabsWindow::Render() const`.
WindowMember(TabsWindow);

WindowMember(
    ApplicationSettings,
    Prop(Float, GestureDurationSec, 0.5, 0, 5); // Merge actions occurring in short succession into a single gesture
);

WindowMember_(
    StateViewer,
    Menu({
        Menu("Settings", {AutoSelect, LabelMode}),
        Menu({}), // Need multiple elements to disambiguate vector-of-variants construction from variant construction.
    }),
    enum LabelMode{Annotated, Raw};
    Prop_(Enum, LabelMode, "?The raw JSON state doesn't store keys for all items.\n"
                           "For example, the main `ui.style.colors` state is a list.\n\n"
                           "'Annotated' mode shows (highlighted) labels for such state items.\n"
                           "'Raw' mode shows the state exactly as it is in the raw JSON state.",
          {"Annotated", "Raw"}, Annotated);
    Prop_(Bool, AutoSelect, "Auto-Select?When auto-select is enabled, state changes automatically open.\n"
                            "The state viewer to the changed state node(s), closing all other state nodes.\n"
                            "State menu items can only be opened or closed manually if auto-select is disabled.",
          true);

    void StateJsonTree(string_view key, const json &value, const StatePath &path = RootPath) const;
);

WindowMember_(StateMemoryEditor, ImGuiWindowFlags_NoScrollbar);
WindowMember(StatePathUpdateFrequency);

enum ProjectFormat {
    StateFormat,
    ActionFormat
};
WindowMember(
    ProjectPreview,
    Prop(Enum, Format, {"StateFormat", "ActionFormat"}, 1);
    Prop(Bool, Raw)
);

enum AudioBackend {
    none,
    dummy,
    alsa,
    pulseaudio,
    jack,
    coreaudio,
    wasapi
};

// Starting at `-1` allows for using `IO` types as array indices.
enum IO_ {
    IO_None = -1,
    IO_In,
    IO_Out
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

WindowMember(
    Audio,
    // A selection of supported formats, corresponding to `SoundIoFormat`
    enum IoFormat_{
        IoFormat_Invalid = 0,
        IoFormat_Float64NE,
        IoFormat_Float32NE,
        IoFormat_S32NE,
        IoFormat_S16NE,
    };
    using IoFormat = int;

    static const vector<IoFormat> PrioritizedDefaultFormats;
    static const vector<int> PrioritizedDefaultSampleRates;

    // todo state member & respond to changes, or remove from state
    UIMember(
        FaustState,
        WindowMember_(
            FaustEditor,
            ImGuiWindowFlags_MenuBar,

            string FileName{"default.dsp"};
        );

        WindowMember_(
            FaustDiagram,
            Menu({
                Menu("File", {ShowSaveFaustSvgFileDialog{}}),
                Menu("View", {Settings.HoverFlags}),
            }),

            Member(
                DiagramSettings,
                Prop_(
                    Flags, HoverFlags,
                    "?Hovering over a node in the graph will display the selected information",
                    {"ShowRect?Display the hovered node's bounding rectangle",
                     "ShowType?Display the hovered node's box type",
                     "ShowChannels?Display the hovered node's channel points and indices",
                     "ShowChildChannels?Display the channel points and indices for each of the hovered node's children"},
                    FaustDiagramHoverFlags_None
                )
            );
            Prop(DiagramSettings, Settings);
        );

        WindowMember(FaustParams);
        WindowMember(
            FaustLog,
            Prop(String, Error);
        );

        Prop_(FaustEditor, Editor, "Faust editor");
        Prop_(FaustDiagram, Diagram, "Faust diagram");
        Prop_(FaustParams, Params, "Faust params");
        Prop_(FaustLog, Log, "Faust log");

        //        Prop(String, Code, R"#(import("stdfaust.lib");
        // pitchshifter = vgroup("Pitch Shifter", ef.transpose(
        //    vslider("window (samples)", 1000, 50, 10000, 1),
        //    vslider("xfade (samples)", 10, 1, 10000, 1),
        //    vslider("shift (semitones)", 0, -24, +24, 0.1)
        //  )
        //);
        // process = _ : pitchshifter;)#");
        //        Prop(String, Code, R"#(import("stdfaust.lib");
        // s = vslider("Signal[style:radio{'Noise':0;'Sawtooth':1}]",0,0,1,1);
        // process = select2(s,no.noise,os.sawtooth(440));)#");
        //        Prop(String, Code, R"(import("stdfaust.lib");
        // process = ba.beat(240) : pm.djembe(60, 0.3, 0.4, 1) <: dm.freeverb_demo;)");
        //        Prop(String, Code, R"(import("stdfaust.lib");
        // process = _:fi.highpass(2,1000):_;)");
        //        Prop(String, Code, R"(import("stdfaust.lib");
        // ctFreq = hslider("cutoffFrequency",500,50,10000,0.01);
        // q = hslider("q",5,1,30,0.1);
        // gain = hslider("gain",1,0,1,0.01);
        // process = no:noise : fi.resonlp(ctFreq,q,gain);");

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
    );

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
    Prop(Enum, InFormat, {"Invalid", "Float64", "Float32", "Short32", "Short16"}, IoFormat_Invalid);
    Prop(Enum, OutFormat, {"Invalid", "Float64", "Float32", "Short32", "Short16"}, IoFormat_Invalid);
    Prop(Float, OutDeviceVolume, 1.0);
    Prop_(Bool, MonitorInput, "?Enabling adds the audio input stream directly to the audio output.");
    Prop(FaustState, Faust);
);

enum FlowGridCol_ {
    FlowGridCol_GestureIndicator, // 2nd series in ImPlot color map (same in all 3 styles for now): `ImPlot::GetColormapColor(1, 0)`
    FlowGridCol_HighlightText, // ImGuiCol_PlotHistogramHovered
    // Params colors.
    FlowGridCol_ParamsBg, // ImGuiCol_FrameBg with less alpha

    FlowGridCol_COUNT
};
using FlowGridCol = int;

enum FlowGridDiagramCol_ {
    FlowGridDiagramCol_Bg, // ImGuiCol_WindowBg
    FlowGridDiagramCol_Text, // ImGuiCol_Text
    FlowGridDiagramCol_DecorateStroke, // ImGuiCol_Border
    FlowGridDiagramCol_GroupStroke, // ImGuiCol_Border
    FlowGridDiagramCol_Line, // ImGuiCol_PlotLines
    FlowGridDiagramCol_Link, // ImGuiCol_Button
    FlowGridDiagramCol_Inverter, // ImGuiCol_Text
    FlowGridDiagramCol_OrientationMark, // ImGuiCol_Text
    // Box fill colors of various types. todo design these colors for Dark/Classic/Light profiles
    FlowGridDiagramCol_Normal,
    FlowGridDiagramCol_Ui,
    FlowGridDiagramCol_Slot,
    FlowGridDiagramCol_Number,

    FlowGridDiagramCol_COUNT
};
using FlowGridDiagramCol = int;

struct Vec2 : UIStateMember {
    // `fmt` defaults to ImGui slider default, which is "%.3f"
    Vec2(StateMember *parent, string_view path_segment, string_view name_help, const ImVec2 &value = {0, 0}, float min = 0, float max = 1, const char *fmt = nullptr)
        : UIStateMember(parent, path_segment, name_help),
          X(this, "X", "", value.x, min, max), Y(this, "Y", "", value.y, min, max),
          min(min), max(max), fmt(fmt) {}

    operator ImVec2() const { return {X, Y}; }

    Float X, Y;
    float min, max; // todo don't need these anymore, derive from X/Y
    const char *fmt;

protected:
    virtual void Render(ImGuiSliderFlags) const;
    void Render() const override;
};

struct Vec2Linked : Vec2 {
    using Vec2::Vec2;
    Vec2Linked(StateMember *parent, string_view path_segment, string_view name_help, const ImVec2 &value = {0, 0}, float min = 0, float max = 1, bool linked = true, const char *fmt = nullptr);

    Prop(Bool, Linked, true);

protected:
    void Render(ImGuiSliderFlags) const override;
    void Render() const override;
};

struct Demo : TabsWindow {
    Demo(StateMember *parent, string_view path_segment, string_view name_help)
        : TabsWindow(parent, path_segment, name_help, ImGuiWindowFlags_MenuBar) {}

    UIMember(ImGuiDemo);
    UIMember(ImPlotDemo);
    UIMember(FileDialogDemo);

    Prop(ImGuiDemo, ImGui);
    Prop(ImPlotDemo, ImPlot);
    Prop(FileDialogDemo, FileDialog);
};

struct Metrics : TabsWindow {
    using TabsWindow::TabsWindow;

    UIMember(FlowGridMetrics, Prop(Bool, ShowRelativePaths, true););
    UIMember(ImGuiMetrics);
    UIMember(ImPlotMetrics);

    Prop(FlowGridMetrics, FlowGrid);
    Prop(ImGuiMetrics, ImGui);
    Prop(ImPlotMetrics, ImPlot);
};

struct Style : TabsWindow {
    using TabsWindow::TabsWindow;

    UIMember_(
        FlowGridStyle,

        UIMember_(
            Diagram,

            Prop_(
                UInt, FoldComplexity,
                "?Number of boxes within a diagram before folding into a sub-diagram.\n"
                "Setting to zero disables folding altogether, for a fully-expanded diagram.",
                3, 0, 20
            );
            Prop_(Bool, ScaleFillHeight, "?Automatically scale to fill the full height of the diagram window, keeping the same aspect ratio.");
            Prop(Float, Scale, 1, 0.1, 5);
            Prop(Enum, Direction, {"Left", "Right"}, ImGuiDir_Right);
            Prop(Bool, RouteFrame);
            Prop(Bool, SequentialConnectionZigzag, false); // `false` uses diagonal lines instead of zigzags instead of zigzags
            Prop(Bool, OrientationMark, false);
            Prop(Float, OrientationMarkRadius, 1.5, 0.5, 3);

            Prop(Bool, DecorateRootNode, false);
            Prop(Vec2Linked, DecorateMargin, {10, 10}, 0, 20);
            Prop(Vec2Linked, DecoratePadding, {10, 10}, 0, 20);
            Prop(Float, DecorateLineWidth, 1, 1, 4);
            Prop(Float, DecorateCornerRadius, 0, 0, 10);

            Prop(Vec2Linked, GroupMargin, {8, 8}, 0, 20);
            Prop(Vec2Linked, GroupPadding, {8, 8}, 0, 20);
            Prop(Float, GroupLineWidth, 2, 1, 4);
            Prop(Float, GroupCornerRadius, 5, 0, 10);

            Prop(Vec2Linked, NodeMargin, {8, 8}, 0, 20);
            Prop(Vec2Linked, NodePadding, {8, 0}, 0, 20, false); // todo padding y not actually used yet, since blocks already have a min-height determined by WireGap.

            Prop(Float, BoxCornerRadius, 4, 0, 10);
            Prop(Float, BinaryHorizontalGapRatio, 0.25, 0, 1);
            Prop(Float, WireWidth, 1, 0.5, 4);
            Prop(Float, WireGap, 16, 10, 20);
            Prop(Vec2, ArrowSize, {3, 2}, 1, 10);
            Prop(Float, InverterRadius, 3, 1, 5);
            Prop(Colors, Colors, GetColorName);

            const vector<std::reference_wrapper<Field::Base>> LayoutFields{
                SequentialConnectionZigzag,
                OrientationMark,
                OrientationMarkRadius,
                DecorateRootNode,
                DecorateMargin.X,
                DecorateMargin.Y,
                DecoratePadding.X,
                DecoratePadding.Y,
                DecorateLineWidth,
                DecorateCornerRadius,
                GroupMargin.X,
                GroupMargin.Y,
                GroupPadding.X,
                GroupPadding.Y,
                GroupLineWidth,
                GroupCornerRadius,
                BoxCornerRadius,
                BinaryHorizontalGapRatio,
                WireWidth,
                WireGap,
                NodeMargin.X,
                NodeMargin.Y,
                NodePadding.X,
                NodePadding.Y,
                ArrowSize.X,
                ArrowSize.Y,
                InverterRadius,
            };
            const FieldEntries DefaultLayoutEntries = LayoutFields | transform([](const Field::Base &field) { return FieldEntry(field, field.GetInitial()); }) | to<const FieldEntries>;

            void ColorsDark(TransientStore &store) const;
            void ColorsClassic(TransientStore &store) const;
            void ColorsLight(TransientStore &store) const;
            void ColorsFaust(TransientStore &store) const; // Color Faust diagrams the same way Faust does when it renders to SVG.

            void LayoutFlowGrid(TransientStore &store) const;
            void LayoutFaust(TransientStore &store) const; // Layout Faust diagrams the same way Faust does when it renders to SVG.

            static const char *GetColorName(FlowGridDiagramCol idx) {
                switch (idx) {
                    case FlowGridDiagramCol_Bg: return "DiagramBg";
                    case FlowGridDiagramCol_DecorateStroke: return "DiagramDecorateStroke";
                    case FlowGridDiagramCol_GroupStroke: return "DiagramGroupStroke";
                    case FlowGridDiagramCol_Line: return "DiagramLine";
                    case FlowGridDiagramCol_Link: return "DiagramLink";
                    case FlowGridDiagramCol_Normal: return "DiagramNormal";
                    case FlowGridDiagramCol_Ui: return "DiagramUi";
                    case FlowGridDiagramCol_Slot: return "DiagramSlot";
                    case FlowGridDiagramCol_Number: return "DiagramNumber";
                    case FlowGridDiagramCol_Inverter: return "DiagramInverter";
                    case FlowGridDiagramCol_OrientationMark: return "DiagramOrientationMark";
                    default: return "Unknown";
                }
            }
        );

        UIMember(
            Params,
            Prop(Bool, HeaderTitles, true);
            // In frame-height units:
            Prop(Float, MinHorizontalItemWidth, 4, 2, 8);
            Prop(Float, MaxHorizontalItemWidth, 16, 10, 24);
            Prop(Float, MinVerticalItemHeight, 4, 2, 8);
            Prop(Float, MinKnobItemSize, 3, 2, 6);

            Prop(Enum, AlignmentHorizontal, {"Left", "Middle", "Right"}, HJustify_Middle);
            Prop(Enum, AlignmentVertical, {"Top", "Middle", "Bottom"}, VJustify_Middle);
            Prop(Flags, TableFlags, TableFlagItems, TableFlags_Borders | TableFlags_Reorderable | TableFlags_Hideable);
            Prop_(
                Enum, WidthSizingPolicy,
                "?StretchFlexibleOnly: If a table contains only fixed-width items, it won't stretch to fill available width.\n"
                "StretchToFill: If a table contains only fixed-width items, allow columns to stretch to fill available width.\n"
                "Balanced: All param types are given flexible-width, weighted by their minimum width. (Looks more balanced, but less expansion room for wide items).",
                {"StretchToFill", "StretchFlexibleOnly", "Balanced"},
                ParamsWidthSizingPolicy_StretchFlexibleOnly
            )
        );

        Prop(Float, FlashDurationSec, 0.6, 0.1, 5);
        Prop(Diagram, Diagram);
        Prop(Params, Params);
        Prop(Colors, Colors, GetColorName);

        void ColorsDark(TransientStore &store) const;
        void ColorsLight(TransientStore &store) const;
        void ColorsClassic(TransientStore &store) const;

        static const char *GetColorName(FlowGridCol idx) {
            switch (idx) {
                case FlowGridCol_GestureIndicator: return "GestureIndicator";
                case FlowGridCol_HighlightText: return "HighlightText";
                case FlowGridCol_ParamsBg: return "ParamsBg";
                default: return "Unknown";
            }
        }
    );

    UIMember_(
        ImGuiStyle,

        void Apply(ImGuiContext *ctx) const;
        void ColorsDark(TransientStore &) const;
        void ColorsLight(TransientStore &) const;
        void ColorsClassic(TransientStore &) const;

        static constexpr float FontAtlasScale = 2; // We rasterize to a scaled-up texture and scale down the font size globally, for sharper text.

        // See `ImGui::ImGuiStyle` for field descriptions.
        // Initial values copied from `ImGui::ImGuiStyle()` default constructor.
        // Ranges copied from `ImGui::StyleEditor`.
        // Double-check everything's up-to-date from time to time!

        // Main
        Prop(Vec2Linked, WindowPadding, {8, 8}, 0, 20, "%.0f");
        Prop(Vec2Linked, FramePadding, {4, 3}, 0, 20, false, "%.0f");
        Prop(Vec2Linked, CellPadding, {4, 2}, 0, 20, false, "%.0f");
        Prop(Vec2, ItemSpacing, {8, 4}, 0, 20, "%.0f");
        Prop(Vec2Linked, ItemInnerSpacing, {4, 4}, 0, 20, true, "%.0f");
        Prop(Vec2Linked, TouchExtraPadding, {0, 0}, 0, 10, true, "%.0f");
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
        Prop(Vec2, WindowTitleAlign, {0, 0.5}, 0, 1, "%.2f");
        Prop(Enum, WindowMenuButtonPosition, {"Left", "Right"}, ImGuiDir_Left);
        Prop(Enum, ColorButtonPosition, {"Left", "Right"}, ImGuiDir_Right);
        Prop_(Vec2Linked, ButtonTextAlign, "?Alignment applies when a button is larger than its text content.", {0.5, 0.5}, 0, 1, "%.2f");
        Prop_(Vec2Linked, SelectableTextAlign, "?Alignment applies when a selectable is larger than its text content.", {0, 0}, 0, 1, "%.2f");

        // Safe area padding
        Prop_(Vec2Linked, DisplaySafeAreaPadding, "?Adjust if you cannot see the edges of your screen (e.g. on a TV where scaling has not been configured).", {3, 3}, 0, 30, "%.0f");

        // Rendering
        Prop_(Bool, AntiAliasedLines, "Anti-aliased lines?When disabling anti-aliasing lines, you'll probably want to disable borders in your style as well.", true);
        Prop_(Bool, AntiAliasedLinesUseTex, "Anti-aliased lines use texture?Faster lines using texture data. Require backend to render with bilinear filtering (not point/nearest filtering).", true);
        Prop_(Bool, AntiAliasedFill, "Anti-aliased fill", true);
        Prop_(Float, CurveTessellationTol, "Curve tesselation tolerance", 1.25, 0.1, 10, "%.2f", ImGuiSliderFlags_None, 0.02f);
        Prop(Float, CircleTessellationMaxError, 0.3, 0.1, 5, "%.2f");
        Prop(Float, Alpha, 1, 0.2, 1, "%.2f", ImGuiSliderFlags_None, 0.005); // Not exposing zero here so user doesn't "lose" the UI (zero alpha clips all widgets).
        Prop_(Float, DisabledAlpha, "?Additional alpha multiplier for disabled items (multiply over current value of Alpha).", 0.6, 0, 1, "%.2f", ImGuiSliderFlags_None, 0.005);

        // Fonts
        Prop(Int, FontIndex);
        Prop_(Float, FontScale, "?Global font scale (low-quality!)", 1, 0.3, 2, "%.2f", ImGuiSliderFlags_AlwaysClamp, 0.005f);

        // Not editable todo delete?
        Prop(Float, TabMinWidthForCloseButton, 0);
        Prop(Vec2Linked, DisplayWindowPadding, {19, 19});
        Prop(Vec2, WindowMinSize, {32, 32});
        Prop(Float, MouseCursorScale, 1);
        Prop(Float, ColumnsMinSpacing, 6);

        Prop(Colors, Colors, ImGui::GetStyleColorName);
    );

    UIMember_(
        ImPlotStyle,

        void Apply(ImPlotContext *ctx) const;
        void ColorsAuto(TransientStore &store) const;
        void ColorsDark(TransientStore &store) const;
        void ColorsLight(TransientStore &store) const;
        void ColorsClassic(TransientStore &store) const;

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
        Prop(Vec2Linked, MajorTickLen, {10, 10}, 0, 20, "%.0f");
        Prop(Vec2Linked, MinorTickLen, {5, 5}, 0, 20, "%.0f");
        Prop(Vec2Linked, MajorTickSize, {1, 1}, 0, 2, "%.1f");
        Prop(Vec2Linked, MinorTickSize, {1, 1}, 0, 2, "%.1f");
        Prop(Vec2Linked, MajorGridSize, {1, 1}, 0, 2, "%.1f");
        Prop(Vec2Linked, MinorGridSize, {1, 1}, 0, 2, "%.1f");
        Prop(Vec2, PlotDefaultSize, {400, 300}, 0, 1000, "%.0f");
        Prop(Vec2, PlotMinSize, {200, 150}, 0, 300, "%.0f");

        // Plot padding
        Prop(Vec2Linked, PlotPadding, {10, 10}, 0, 20, "%.0f");
        Prop(Vec2Linked, LabelPadding, {5, 5}, 0, 20, "%.0f");
        Prop(Vec2Linked, LegendPadding, {10, 10}, 0, 20, "%.0f");
        Prop(Vec2Linked, LegendInnerPadding, {5, 5}, 0, 10, "%.0f");
        Prop(Vec2, LegendSpacing, {5, 0}, 0, 5, "%.0f");
        Prop(Vec2Linked, MousePosPadding, {10, 10}, 0, 20, "%.0f");
        Prop(Vec2Linked, AnnotationPadding, {2, 2}, 0, 5, "%.0f");
        Prop(Vec2Linked, FitPadding, {0, 0}, 0, 0.2, "%.2f");

        Prop(Colors, Colors, ImPlot::GetStyleColorName, true);
        Prop(Bool, UseLocalTime);
        Prop(Bool, UseISO8601);
        Prop(Bool, Use24HourClock);

        Prop(Int, Marker, ImPlotMarker_None); // Not editable todo delete?
    );

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
Member(
    DockNodeSettings,

    void Set(const ImVector<ImGuiDockNodeSettings> &, TransientStore &) const;
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
);

Member(
    WindowSettings,

    void Set(ImChunkStream<ImGuiWindowSettings> &, TransientStore &) const;
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
);

Member(
    TableColumnSettings,
    // [table_index][column_index]
    Prop(Vector2D<float>, WidthOrWeight);
    Prop(Vector2D<ID>, UserID);
    Prop(Vector2D<int>, Index);
    Prop(Vector2D<int>, DisplayOrder);
    Prop(Vector2D<int>, SortOrder);
    Prop(Vector2D<int>, SortDirection);
    Prop(Vector2D<bool>, IsEnabled); // "Visible" in ini file
    Prop(Vector2D<bool>, IsStretch);
);

Member(
    TableSettings,

    void Set(ImChunkStream<ImGuiTableSettings> &, TransientStore &store) const;
    void Apply(ImGuiContext *) const;

    Prop(Vector<ImGuiID>, ID);
    Prop(Vector<int>, SaveFlags);
    Prop(Vector<float>, RefScale);
    Prop(Vector<Count>, ColumnsCount);
    Prop(Vector<Count>, ColumnsCountMax);
    Prop(Vector<bool>, WantApply);
    Prop(TableColumnSettings, Columns);
);

Member(
    ImGuiSettings,

    Store Set(ImGuiContext *ctx) const;
    // Inverse of above constructor. `imgui_context.settings = this`
    // Should behave just like `ImGui::LoadIniSettingsFromMemory`, but using the structured `...Settings` members
    //  in this struct instead of the serialized .ini text format.
    void Apply(ImGuiContext *ctx) const;

    Prop(DockNodeSettings, Nodes);
    Prop(WindowSettings, Windows);
    Prop(TableSettings, Tables);
);

WindowMember(Info);
WindowMember(StackTool);
WindowMember(DebugLog);

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

WindowMember_(
    FileDialog, false, // File dialogs are not visible by default.
    void Set(const FileDialogData &data, TransientStore &) const;

    Prop(Bool, SaveMode); // The same file dialog instance is used for both saving & opening files.
    Prop(Int, MaxNumSelections, 1);
    Prop(Int, Flags, FileDialogFlags_Default);
    Prop(String, Title, "Choose file");
    Prop(String, Filters);
    Prop(String, FilePath, ".");
    Prop(String, DefaultFileName);
);

//-----------------------------------------------------------------------------
// [SECTION] Main application `State`
//-----------------------------------------------------------------------------
struct OpenRecentProject : MenuItemDrawable {
    void MenuItem() const override;
};

UIMember(
    State,

    OpenRecentProject open_recent_project{};
    const Menu MainMenu{
        {
            Menu("File", {OpenEmptyProject{}, ShowOpenProjectDialog{}, open_recent_project, OpenDefaultProject{}, SaveCurrentProject{}, SaveDefaultProject{}}),
            Menu("Edit", {Undo{}, Redo{}}),
            Menu(
                "Windows",
                {
                    Menu("Debug", {DebugLog, StackTool, StateViewer, StatePathUpdateFrequency, StateMemoryEditor, ProjectPreview}),
                    Menu("Audio", {Audio.Faust.Editor, Audio.Faust.Diagram, Audio.Faust.Params, Audio.Faust.Log}),
                    Metrics,
                    Style,
                    Demo,
                }
            ),
        },
        true};

    void Update(const StateAction &, TransientStore &) const;
    void Apply(UIContext::Flags) const;

    WindowMember_(
        UIProcess,
        false,
        Prop_(Bool, Running, format("?Disabling ends the {} process.\nEnabling will start the process up again.", Lowercase(Name)), true);
    );

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
);

//-----------------------------------------------------------------------------
// [SECTION] Main application `Context`
//-----------------------------------------------------------------------------

static const map<ProjectFormat, string> ExtensionForProjectFormat{{StateFormat, ".fls"}, {ActionFormat, ".fla"}};
static const auto ProjectFormatForExtension = ExtensionForProjectFormat | transform([](const auto &p) { return pair(p.second, p.first); }) | to<map>();
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

enum Direction {
    Forward,
    Reverse
};

struct StoreHistory {
    struct Record {
        const TimePoint Committed;
        const Store Store; // The store as it was at `Committed` time
        const Gesture Gesture; // Compressed gesture (list of `ActionMoment`s) that caused the store change
    };
    struct Plottable {
        vector<const char *> Labels;
        vector<ImU64> Values;
    };

    StoreHistory(const Store &store) : Records{{Clock::now(), store, {}}} {}

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
    Gesture ActiveGesture{}; // uncompressed, uncommitted
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
    Patch SetStore(const Store &);

    TransientStore InitStore{}; // Used in `StateMember` constructors to initialize the store.

private:
    const State ApplicationState{};
    Store ApplicationStore{InitStore.persistent()}; // Create the local canonical store, initially containing the full application state constructed by `State`.

public:
    const State &s = ApplicationState;
    const Store &AppStore = ApplicationStore;

    Preferences Preferences;
    StoreHistory History{AppStore}; // One store checkpoint for every gesture.
    bool ProjectHasChanges{false};

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

// Persistent (immutable) store setters
Store Set(const Field::Base &, const Primitive &, const Store &store = AppStore);
Store Set(const StoreEntries &, const Store &store = AppStore);
Store Set(const FieldEntries &, const Store &store = AppStore);

// Equivalent setters for a transient (mutable) store
void Set(const Field::Base &, const Primitive &, TransientStore &);
void Set(const StoreEntries &, TransientStore &);
void Set(const FieldEntries &, TransientStore &);

Patch CreatePatch(const Store &before, const Store &after, const StatePath &BasePath = RootPath);
