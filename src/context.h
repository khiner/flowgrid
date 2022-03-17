#pragma once

#include <iostream>

#include "action.h"
#include "state_renderers/render_json.h"
#include "visitor.h"

using namespace action;

struct Context {
    const State &state = _state; // Read-only public state
    void update(Action);
private:
    State _state{};
};

extern Context context;
