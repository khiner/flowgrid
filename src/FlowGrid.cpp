#include "FlowGrid.h"

void FlowGrid::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](Action::Audio::Any &&a) { Audio.Apply(std::move(a)); },
        },
        action
    );
}

bool FlowGrid::CanApply(const ActionType &action) const {
    return std::visit(
        Match{
            [this](Action::Audio::Any &&a) { return Audio.CanApply(std::move(a)); },
        },
        action
    );
}
