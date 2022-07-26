#pragma once

#include <string>
#include <variant>

#include "State.h"
#include "Helper/String.h"

/*!
 * From [lager](https://github.com/arximboldi/lager/blob/c9d8b7d3c7dc7138913757d1624ab705866d791d/lager/util.hpp#L27-L49)
 * Utility to make a variant visitor out of lambdas, using the *overloaded pattern* as described
 * [here](https://en.cppreference.com/w/cpp/utility/variant/visit).
 */
template<class... Ts>
struct visitor : Ts ... {
    using Ts::operator()...;
};

template<class... Ts> visitor(Ts...)->visitor<Ts...>;


/**
 An `Action` is an immutable representation of a user interaction event.
 Each action stores all information needed for `update` to apply it to the global `State` instance.

 Conventions:
 * Static members
   - _Not relevant as there aren't any static members atm, but note to future-self._
   - Any static members should be prefixed with an underscore to avoid name collisions with data members.
   - All such static members are declared _after_ data members, to allow for default construction using only non-static members.

 Note that adding static members does not increase the size of the parent `Action` variant.
 (You can verify this by looking at the 'Action variant size' in the FlowGrid metrics window.)
*/

namespace actions {

struct end_gesture { bool merge; }; // This only exists as a gesture marker to allow saving/loading projects as a list of actions.

struct undo {};
struct redo {};

struct open_project { string path; };
struct open_empty_project {};
struct open_default_project {};
struct show_open_project_dialog {};

struct open_file_dialog { File::DialogData dialog; };
struct close_file_dialog {};

struct save_project { string path; };
struct save_current_project {};
struct save_default_project {};
struct show_save_project_dialog {};

struct close_application {};

struct set_value { string state_path; json value; };

// JSON types are used for actions that hold very large structured data.
// This is because the `Action` `std::variant` below can hold any action type, and variants must be large enough to hold their largest type.
// As of 5/24/2022, the largest raw action member type was `ImGuiStyle`, which resulted in an `Action` variant size of 1088 bytes.
// That's pretty silly for a type that can also hold a single boolean value! Replacing with JSON types brought the size down to 32 bytes.
struct set_imgui_settings { json settings; }; // ImGuiSettings
struct set_imgui_color_style { int id; };
struct set_implot_color_style { int id; };
struct set_flowgrid_color_style { int id; };

struct close_window { string name; };
struct toggle_window { string name; };

struct toggle_state_viewer_auto_select {};
struct set_state_viewer_label_mode { StateViewer::LabelMode label_mode; };

struct set_audio_sample_rate { int sample_rate; };

struct set_ui_running { bool running; };

struct show_open_faust_file_dialog {};
struct show_save_faust_file_dialog {};
struct save_faust_file { string path; };
struct open_faust_file { string path; };
struct set_faust_code { string text; };


EmptyJsonType(undo)
EmptyJsonType(redo)
EmptyJsonType(open_empty_project)
EmptyJsonType(open_default_project)
EmptyJsonType(show_open_project_dialog)
EmptyJsonType(close_file_dialog)
EmptyJsonType(save_current_project)
EmptyJsonType(save_default_project)
EmptyJsonType(show_save_project_dialog)
EmptyJsonType(close_application)
EmptyJsonType(toggle_state_viewer_auto_select)
EmptyJsonType(show_open_faust_file_dialog)
EmptyJsonType(show_save_faust_file_dialog)

JsonType(end_gesture, merge)
JsonType(open_project, path)
JsonType(open_file_dialog, dialog)
JsonType(save_project, path)
JsonType(set_value, state_path, value)
JsonType(set_imgui_settings, settings)
JsonType(set_imgui_color_style, id)
JsonType(set_implot_color_style, id)
JsonType(set_flowgrid_color_style, id)
JsonType(close_window, name)
JsonType(toggle_window, name)
JsonType(set_state_viewer_label_mode, label_mode)
JsonType(set_audio_sample_rate, sample_rate)
JsonType(set_ui_running, running)
JsonType(save_faust_file, path)
JsonType(open_faust_file, path)
JsonType(set_faust_code, text)

}

using namespace actions;

namespace action {

using ID = size_t;

using Action = std::variant<
    end_gesture,
    undo, redo,
    open_project, open_empty_project, open_default_project, show_open_project_dialog,
    save_project, save_default_project, save_current_project, show_save_project_dialog,
    open_file_dialog, close_file_dialog,
    close_application,

    set_value,

    set_imgui_settings, set_imgui_color_style, set_implot_color_style, set_flowgrid_color_style,

    close_window, toggle_window,

    toggle_state_viewer_auto_select, set_state_viewer_label_mode,

    set_audio_sample_rate,
    set_faust_code,
    open_faust_file, save_faust_file,
    show_open_faust_file_dialog, show_save_faust_file_dialog,

    set_ui_running
>;

// Default-construct an action by its variant index (which is also its `ID`).
// From https://stackoverflow.com/a/60567091/780425
// TODO or just this instead?
//  set_faust_code r = std::variant_alternative_t<action_type_index<set_faust_code>, Action>{"foo"};
template<ID I = 0>
Action create(ID index) {
    if constexpr(I >= std::variant_size_v<Action>) throw std::runtime_error{"Action index " + std::to_string(I + index) + " out of bounds"};
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

static const std::map<ID, string> name_for_id{
    {id<end_gesture>,                     ActionName(end_gesture)},
    {id<undo>,                            ActionName(undo)},
    {id<redo>,                            ActionName(redo)},

    {id<open_project>,                    ActionName(open_project)},
    {id<open_empty_project>,              ActionName(open_empty_project)},
    {id<open_default_project>,            ActionName(open_default_project)},
    {id<show_open_project_dialog>,        ActionName(show_open_project_dialog)},

    {id<open_file_dialog>,                ActionName(open_file_dialog)},
    {id<close_file_dialog>,               ActionName(close_file_dialog)},

    {id<save_project>,                    ActionName(save_project)},
    {id<save_default_project>,            ActionName(save_default_project)},
    {id<save_current_project>,            ActionName(save_current_project)},
    {id<show_save_project_dialog>,        ActionName(show_save_project_dialog)},

    {id<close_application>,               ActionName(close_application)},

    {id<set_value>,                       ActionName(set_value)},

    {id<set_imgui_settings>,          "Set ImGui settings"},
    {id<set_imgui_color_style>,       "Set ImGui color style"},
    {id<set_implot_color_style>,      "Set ImPlot color style"},
    {id<set_flowgrid_color_style>,    "Set FlowGrid color style"},

    {id<close_window>,                    ActionName(close_window)},
    {id<toggle_window>,                   ActionName(toggle_window)},

    {id<toggle_state_viewer_auto_select>, ActionName(toggle_state_viewer_auto_select)},
    {id<set_state_viewer_label_mode>, "Set state-viewer label-mode"},
    {id<set_audio_sample_rate>,           ActionName(set_audio_sample_rate)},
    {id<set_faust_code>,              "Set Faust code"},
    {id<show_open_faust_file_dialog>, "Show open Faust file dialog"},
    {id<show_save_faust_file_dialog>, "Show save Faust file dialog"},
    {id<open_faust_file>,             "Open Faust file"},
    {id<save_faust_file>,             "Save Faust file"},

    {id<set_ui_running>,              "Set UI running"},
};

// An action's menu label is its name, except for a few exceptions.
static const std::map<ID, string> menu_label_for_id{
    {id<show_open_project_dialog>,    "Open project"},
    {id<open_empty_project>,          "New project"},
    {id<save_current_project>,        "Save project"},
    {id<show_save_project_dialog>,    "Save project as..."},
    {id<show_open_faust_file_dialog>, "Open DSP file"},
    {id<show_save_faust_file_dialog>, "Save DSP as..."},
};

const std::map<ID, string> shortcut_for_id = {
    {id<undo>,                     "cmd+z"},
    {id<redo>,                     "shift+cmd+z"},
    {id<open_empty_project>,       "cmd+n"},
    {id<show_open_project_dialog>, "cmd+o"},
    {id<save_current_project>,     "cmd+s"},
    {id<open_default_project>,     "shift+cmd+o"},
    {id<save_default_project>,     "shift+cmd+s"},
};

static ID get_id(const Action &action) { return action.index(); }
static string get_name(const Action &action) { return name_for_id.at(get_id(action)); }

static const char *get_menu_label(ID action_id) {
    if (menu_label_for_id.contains(action_id)) return menu_label_for_id.at(action_id).c_str();
    return name_for_id.at(action_id).c_str();
}

}

using ActionID = action::ID;
