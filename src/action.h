#pragma once

#include <variant>
#include "state.h"

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
 Each action stores all information needed for `update` to apply it to a given `State` instance.

 Conventions:
 * Static members
   - _Not relevant as there aren't any static members atm, but note to future-self._
   - Any static members should be prefixed with an underscore to avoid name collisions with data members.
   - All such static members are declared _after_ data members, to allow for default construction using only non-static members.

 Note that adding static members does not increase the size of the parent `Action` variant.
 (You can verify this by looking at the 'Action variant size' in the FlowGrid metrics window.)
*/

namespace actions {

struct undo {};
struct redo {};
struct open_project { string path; };
struct save_project { string path; };
struct save_current_project {};
struct open_empty_project {};
struct open_default_project {};
struct save_default_project {};
struct close_application {};

// JSON types are used for actions that hold very large structured data.
// This is because the `Action` `std::variant` below can hold any action type, and variants must be large enough to hold their largest type.
// As of 5/24/2022, the largest raw action member type was `ImGuiStyle`, which resulted in an `Action` variant size of 1088 bytes.
// That's pretty silly for a type that can also hold a single boolean value! Replacing with JSON types brought the size down to 32 bytes.
struct set_imgui_settings { json settings; }; // ImGuiSettings
struct set_imgui_style { json imgui_style; }; // ImGuiStyle
struct set_implot_style { json implot_style; }; // ImPlotStyle
struct set_flowgrid_style { json flowgrid_style; }; // FlowGridStyle

struct toggle_window { string name; };

struct toggle_state_viewer_auto_select {};
struct set_state_viewer_label_mode { State::StateWindows::StateViewer::LabelMode label_mode; };

struct toggle_audio_muted {};
struct set_audio_sample_rate { int sample_rate; };

struct set_audio_running { bool running; };
struct toggle_audio_running {};
struct set_ui_running { bool running; };

struct set_faust_code { string text; };

}

using namespace actions;

namespace action {

using ID = size_t;

using Action = std::variant<
    undo, redo,
    open_project, open_empty_project, open_default_project,
    save_project, save_default_project, save_current_project,
    close_application,

    set_imgui_settings,
    set_imgui_style, set_implot_style, set_flowgrid_style,

    toggle_window,

    toggle_state_viewer_auto_select,
    set_state_viewer_label_mode,

    toggle_audio_muted,
    set_audio_sample_rate,
    set_faust_code,

    set_audio_running,
    toggle_audio_running,
    set_ui_running
>;

// Default-construct an action by its variant index (which is also its `_id`).
// From https://stackoverflow.com/a/60567091/780425
// TODO or just this instead?
//  set_faust_code r = std::variant_alternative_t<action_type_index<set_faust_code>, Action>{"foo"};
template<std::size_t I = 0>
Action create(std::size_t index) {
    if constexpr(I >= std::variant_size_v<Action>) throw std::runtime_error{"Action index " + std::to_string(I + index) + " out of bounds"};
    else return index == 0 ? Action{std::in_place_index<I>} : create<I + 1>(index - 1);
}

// Get the index for action variant type.
// From https://stackoverflow.com/a/66386518/780425

#include "boost/mp11/mp_find.h"

// E.g. `const ActionId action_id = id<action>`
// An action's ID is its index in the `Action` variant.
// Down the road, this means `Action` would need to be append-only (no order changes) for backwards compatibility.
// Not worried about that right now, since that should be an easy piece to replace with some uuid system later.
// Index is simplest.
template<typename T>
constexpr size_t id = mp_find<Action, T>::value;

static const std::map<ID, string> named{
    {id<undo>,                            "Undo"},
    {id<redo>,                            "Redo"},
    {id<open_project>,                    "Open project"},
    {id<open_empty_project>,              "Open empty project"},
    {id<open_default_project>,            "Open default project"},
    {id<save_project>,                    "Save project"},
    {id<save_default_project>,            "Save default project"},
    {id<save_current_project>,            "Save current project"},
    {id<close_application>,               "Close application"},
    {id<set_imgui_settings>,              "Set ImGui Settings"},
    {id<set_imgui_style>,                 "Set ImGui Style"},
    {id<set_implot_style>,                "Set ImPlot Style"},
    {id<set_flowgrid_style>,              "Set FlowGrid Style"},
    {id<toggle_window>,                   "Toggle Window"},
    {id<toggle_state_viewer_auto_select>, "Toggle state viewer auto-select"},
    {id<set_state_viewer_label_mode>,     "Set state-viewer label-mode"},
    {id<toggle_audio_muted>,              "Toggle audio muted"},
    {id<set_audio_sample_rate>,           "Set audio sample rate"},
    {id<set_faust_code>,                  "Set faust code"},
    {id<set_audio_running>,               "Set audio running"},
    {id<toggle_audio_running>,            "Toggle audio running"},
    {id<set_ui_running>,                  "Set UI running"},
};

static ID get_id(const Action &action) { return action.index(); }
static string get_name(const Action &action) { return named.at(get_id(action)); }

}

using ActionID = action::ID;
using Action = action::Action;
