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
 * An `Action` is an immutable representation of a user interaction event.
 * Each action stores all information needed for `update` to apply it to a given `State` instance.
 *
 * Static members are prefixed with an underscore to avoid name collisions with data members.
 * All such static members are declared _after_ data members, to allow for default construction using only non-static members.
 * Note that adding static members does not increase the size of the `Action` variant of which the `action::` structs are types.
 * (You can verify this by looking at the 'Action variant size' in the FlowGrid metrics window.)
*/

namespace action {

struct undo { static string _name; };
struct redo { static string _name; };
struct open_project { string path; static string _name; };
struct save_project { string path; static string _name; };
struct save_current_project { static string _name; };
struct open_empty_project { static string _name; };
struct open_default_project { static string _name; };
struct save_default_project { static string _name; };
struct close_application { static string _name; };

// JSON types are used for actions that hold very large structured data.
// This is because the `Action` `std::variant` below can hold any action type, and variants must be large enough to hold their largest type.
// As of 5/24/2022, the largest raw action member type was `ImGuiStyle`, which resulted in an `Action` variant size of 1088 bytes.
// That's pretty silly for a type that can also hold a single boolean value! Replacing with JSON types brought the size down to 32 bytes.
struct set_imgui_settings { json settings; static string _name; }; // ImGuiSettings
struct set_imgui_style { json imgui_style; static string _name; }; // ImGuiStyle
struct set_implot_style { json implot_style; static string _name; }; // ImPlotStyle
struct set_flowgrid_style { json flowgrid_style; static string _name; }; // FlowGridStyle

struct toggle_window { string name; static string _name; };

struct toggle_state_viewer_auto_select { static string _name; };
struct set_state_viewer_label_mode { State::StateWindows::StateViewer::LabelMode label_mode; static string _name; };

struct toggle_audio_muted { static string _name; };
struct set_audio_sample_rate { int sample_rate; static string _name; };

struct set_audio_running { bool running; static string _name; };
struct toggle_audio_running { static string _name; };
struct set_ui_running { bool running; static string _name; };

struct set_faust_code { string text; static string _name; };

}

using namespace action;

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

string get_action_name(const Action &);
