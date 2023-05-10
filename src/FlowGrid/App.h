#pragma once

#include "nlohmann/json_fwd.hpp"

#include "Audio/Audio.h"
#include "FileDialog/FileDialog.h"
#include "ImGuiSettings.h"
#include "Style.h"

/**
 * This class defines the main `State`, which fully describes the application at any point in time.
 * An immutable reference to the single source-of-truth application state `const State &s` is defined at the bottom of this file.
 */

using namespace nlohmann;

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
    Prop_(Enum, LabelMode, "?The raw dog JSON state doesn't store keys for all items.\n"
                           "For example, the main `ui.style.colors` state is a list.\n\n"
                           "'Annotated' mode shows (highlighted) labels for such state items.\n"
                           "'Raw' mode shows the state exactly as it is in the raw JSON state.",
          {"Annotated", "Raw"}, Annotated);
    Prop_(Bool, AutoSelect, "Auto-Select?When auto-select is enabled, state changes automatically open.\n"
                            "The state viewer to the changed state node(s), closing all other state nodes.\n"
                            "State menu items can only be opened or closed manually if auto-select is disabled.",
          true);

    void StateJsonTree(string_view key, const json &value, const StorePath &path = RootPath) const;
);

WindowMember_(StateMemoryEditor, WindowFlags_NoScrollbar);
WindowMember(StorePathUpdateFrequency);

WindowMember(
    ProjectPreview,
    Prop(Enum, Format, {"StateFormat", "ActionFormat"}, 1);
    Prop(Bool, Raw)
);

struct Demo : TabsWindow {
    Demo(StateMember *parent, string_view path_segment, string_view name_help);

    UIMember(ImGuiDemo);
    UIMember(ImPlotDemo);

    Prop(ImGuiDemo, ImGui);
    Prop(ImPlotDemo, ImPlot);
    Prop(FileDialog::Demo, FileDialog);
};

struct Metrics : TabsWindow {
    using TabsWindow::TabsWindow;

    UIMember(FlowGridMetrics, Prop(Bool, ShowRelativePaths, true));
    UIMember(ImGuiMetrics);
    UIMember(ImPlotMetrics);

    Prop(FlowGridMetrics, FlowGrid);
    Prop(ImGuiMetrics, ImGui);
    Prop(ImPlotMetrics, ImPlot);
};

WindowMember(Info);
WindowMember(StackTool);
WindowMember(DebugLog);

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
                    Menu("Debug", {DebugLog, StackTool, StateViewer, StorePathUpdateFrequency, StateMemoryEditor, ProjectPreview}),
                    Menu(
                        "Faust",
                        {
                            Menu("Editor", {Audio.Faust.Editor, Audio.Faust.Editor.Metrics}),
                            Audio.Faust.Graph,
                            Audio.Faust.Params,
                            Audio.Faust.Log,
                        }
                    ),
                    Audio,
                    Metrics,
                    Style,
                    Demo,
                }
            ),
        },
        true};

    void Update(const StateAction &, TransientStore &) const;

    WindowMember_(
        UIProcess,
        false,
        Prop_(Bool, Running, std::format("?Disabling ends the {} process.\nEnabling will start the process up again.", Name), true);
    );

    Prop(ImGuiSettings, ImGuiSettings);
    Prop(fg::Style, Style);
    Prop(Audio, Audio);
    Prop(ApplicationSettings, ApplicationSettings);
    Prop(UIProcess, UiProcess);
    Prop(FileDialog, FileDialog);
    Prop(Info, Info);

    Prop(Demo, Demo);
    Prop(Metrics, Metrics);
    Prop(StackTool, StackTool);
    Prop(::DebugLog, DebugLog);

    Prop(StateViewer, StateViewer);
    Prop(StateMemoryEditor, StateMemoryEditor);
    Prop(StorePathUpdateFrequency, StorePathUpdateFrequency);
    Prop(ProjectPreview, ProjectPreview);
);

/**
Declare global read-only accessor for the canonical state instance `s`.

`s` is a read-only structured representation of its underlying store (of type `Store`, which itself is an `immer::map<Path, Primitive>`).
It provides a complete nested struct representation of the state, along with additional metadata about each state member, such as its `Path`/`ID`/`Name`/`Info`.
Basically, it contains all data for each state member except its _actual value_ (a `Primitive`, struct of `Primitive`s, or collection of either).
(Actually, each primitive leaf value is cached on its respective `Field`, but this is a technicality - the `Store` is conceptually the source of truth.)

`s` has an immutable assignment operator, which return a modified copy of the `Store` value resulting from applying the assignment to the provided `Store`.
(Note that this is only _conceptually_ a copy - see [Application Architecture](https://github.com/khiner/flowgrid#application-architecture) for more details.)

Usage example:

```cpp
// Get a read-only reference to the complete, current, structured audio state instance:
const Audio &audio = s.Audio;
```
*/
extern const State &s;
