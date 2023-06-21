#pragma once

#include "AppAction.h"
#include "Audio/Audio.h"
#include "Core/Action/Actionable.h"
#include "Core/Windows.h"
#include "Debug.h"
#include "Demo.h"
#include "FileDialog/FileDialog.h"
#include "ImGuiSettings.h"
#include "Info.h"
#include "Project/ProjectAction.h"
#include "Settings.h"
#include "Style/Style.h"

/**
 * This class defines the main `App`, which fully describes the application at any point in time.
 * An immutable reference to the single source-of-truth application state `const App &app` is defined at the bottom of this file.
 */
struct App : Component, Drawable, Actionable<Action::App::Any> {
    App(ComponentArgs &&);

    static void OpenRecentProjectMenuItem();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    Prop(ImGuiSettings, ImGuiSettings);
    Prop(fg::Style, Style);
    Prop(Audio, Audio);
    Prop(ApplicationSettings, Settings);
    Prop(FileDialog, FileDialog);
    Prop(Info, Info);

    Prop(Demo, Demo);
    Prop(Debug, Debug);

    Prop(Windows, Windows);

    const Menu MainMenu{
        {
            Menu("File", {Action::Project::OpenEmpty::MenuItem, Action::Project::ShowOpenDialog::MenuItem, OpenRecentProjectMenuItem, Action::Project::OpenDefault::MenuItem, Action::Project::SaveCurrent::MenuItem, Action::Project::SaveDefault::MenuItem}),
            Menu("Edit", {Action::Project::Undo::MenuItem, Action::Project::Redo::MenuItem}),
            Windows,
        },
        true};

protected:
    void Render() const override;
};

struct Project {
    static void Init();
    static void SaveEmpty();

    struct ActionHandler : Actionable<Action::Project::Any> {
        void Apply(const ActionType &) const override;
        bool CanApply(const ActionType &) const override;
    };

    inline static ActionHandler ActionHandler;

private:
    static void Open(const fs::path &);
    static bool Save(const fs::path &);
};

void RunQueuedActions(bool force_commit_gesture = false);

/**
Declare global read-only accessor for the canonical state instance `app`.

`app` is a read-only structured representation of its underlying store (of type `Store`, which itself is an `immer::map<Path, Primitive>`).
It provides a complete nested struct representation of the state, along with additional metadata about each state member, such as its `Path`/`ID`/`Name`/`Info`.
Basically, it contains all data for each state member except its _actual value_ (a `Primitive`, struct of `Primitive`s, or collection of either).
(Actually, each primitive leaf value is cached on its respective `Field`, but this is a technicality - the `Store` is conceptually the source of truth.)

`app` has an immutable assignment operator, which return a modified copy of the `Store` value resulting from applying the assignment to the provided `Store`.
(Note that this is only _conceptually_ a copy - see [Application Architecture](https://github.com/khiner/flowgrid#application-architecture) for more details.)
*/
extern const App &app;
