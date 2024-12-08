#include "FlowGrid.h"

void FlowGrid::Apply(TransientStore &s, const ActionType &action) const {
    std::visit(
        Match{
            [this, &s](Action::Audio::Any &&a) { Audio.Apply(s, std::move(a)); },
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

void FlowGrid::FocusDefault() const {
    Audio.Graph.Focus();
    Audio.Faust.Graphs.Focus();
    Audio.Faust.Paramss.Focus();
}
