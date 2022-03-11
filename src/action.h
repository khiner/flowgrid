#pragma once

#include <variant>
#include "state.h"

// An `Action` is an immutable representation of a user interaction event.
// Each action stores all information needed for `update` to apply it to a given `State` instance.

namespace action {

struct toggle_demo_window {};
struct toggle_sine_wave {};
struct set_clear_color { Color color{}; };
struct set_audio_engine_running { bool running; };
struct set_sine_frequency { int frequency; };
struct set_sine_amplitude { float amplitude; };

}

using namespace action;

using UIAction = std::variant<toggle_demo_window, set_clear_color>;
using AudioEngineAction = std::variant<set_audio_engine_running>;
using SineAction = std::variant<
    toggle_sine_wave,
    set_sine_frequency,
    set_sine_amplitude
>;

// Combining variants. Based on https://godbolt.org/z/1a6vnr.
template<typename... A, typename... B>
std::variant<A..., B...> variants_helper(std::variant<A...>, std::variant<B...>);

template<typename A, typename B>
using variants = decltype(variants_helper(std::declval<A>(), std::declval<B>()));

using Action = variants<variants<UIAction, AudioEngineAction>, SineAction>;
