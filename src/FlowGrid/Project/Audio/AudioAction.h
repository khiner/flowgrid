#pragma once

#include "Faust/FaustAction.h"
#include "Graph/AudioGraphAction.h"

DefineActionType(
    Audio,

    using Any = Combine<Faust, AudioGraph::Any>;
);
