#pragma once

#include "FaustDSPAction.h"
#include "FaustGraphAction.h"
#include "FaustGraphStyleAction.h"

DefineActionType(
    Faust,
    using Any = Combine<Faust::DSP::Any, Faust::Graph::Any, Faust::GraphStyle::Any>;
);
