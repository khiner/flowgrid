#pragma once

#include "Audio/Audio.h"
#include "Debug.h"
#include "Demo.h"
#include "FileDialog/FileDialog.h"
#include "ImGuiSettings.h"
#include "Info.h"
#include "Settings.h"
#include "Style.h"

/**
 * This class defines the main `App`, which fully describes the application at any point in time.
 * An immutable reference to the single source-of-truth application state `const App &app` is defined at the bottom of this file.
 */

struct OpenRecentProject : MenuItemDrawable {
    void MenuItem() const override;
};

DefineUI(
    App,

    void Apply(const Action::StatefulAction &) const;

    OpenRecentProject open_recent_project{};
    const Menu MainMenu{
        {
            Menu("File", {Action::OpenEmptyProject::MenuItem, Action::ShowOpenProjectDialog::MenuItem, open_recent_project, Action::OpenDefaultProject::MenuItem, Action::SaveCurrentProject::MenuItem, Action::SaveDefaultProject::MenuItem}),
            Menu("Edit", {Action::Undo::MenuItem, Action::Redo::MenuItem}),
            Menu(
                "Windows",
                {
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
                    Style,
                    Demo,
                    Menu(
                        "Debug",
                        {Debug.Metrics, Debug.DebugLog, Debug.StackTool, Debug.StateViewer, Debug.StorePathUpdateFrequency, Debug.ProjectPreview}
                        // Debug.StateMemoryEditor,
                    ),
                }
            ),
        },
        true};

    Prop(Bool, Running, true);
    Prop(ImGuiSettings, ImGuiSettings);
    Prop(fg::Style, Style);
    Prop(Audio, Audio);
    Prop(ApplicationSettings, Settings);
    Prop(FileDialog, FileDialog);
    Prop(Info, Info);

    Prop(Demo, Demo);
    Prop(Debug, Debug);
);

struct Project {
    static void Init();
    static void SaveEmptyProject();
    static void RunQueuedActions(bool force_finalize_gesture = false);
};

/**
Declare global read-only accessor for the canonical state instance `app`.

`app` is a read-only structured representation of its underlying store (of type `Store`, which itself is an `immer::map<Path, Primitive>`).
It provides a complete nested struct representation of the state, along with additional metadata about each state member, such as its `Path`/`ID`/`Name`/`Info`.
Basically, it contains all data for each state member except its _actual value_ (a `Primitive`, struct of `Primitive`s, or collection of either).
(Actually, each primitive leaf value is cached on its respective `Field`, but this is a technicality - the `Store` is conceptually the source of truth.)

`app` has an immutable assignment operator, which return a modified copy of the `Store` value resulting from applying the assignment to the provided `Store`.
(Note that this is only _conceptually_ a copy - see [Application Architecture](https://github.com/khiner/flowgrid#application-architecture) for more details.)

Usage example:

```cpp
// Get a read-only reference to the complete, current, structured audio state instance:
const Audio &audio = app.Audio;
```
*/
extern const App &app;
