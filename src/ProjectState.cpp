#include "ProjectState.h"

#include "imgui_internal.h"

#include "Project/ProjectContext.h"

using namespace flowgrid;

ProjectState::ProjectState(Store &store, ActionableProducer::Enqueue q, const ::ProjectContext &project_context)
    : Component(store, "ProjectState", project_context), ActionableProducer(std::move(q)) {}

ProjectState::~ProjectState() = default;

void ProjectState::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](ProjectCore::ActionType &&a) { Core.Apply(std::move(a)); },
            [this](FlowGrid::ActionType &&a) { FlowGrid.Apply(std::move(a)); },
        },
        action
    );
}

bool ProjectState::CanApply(const ActionType &action) const {
    return std::visit(
        Match{
            [this](ProjectCore::ActionType &&a) { return Core.CanApply(std::move(a)); },
            [this](FlowGrid::ActionType &&a) { return FlowGrid.CanApply(std::move(a)); },
        },
        action
    );
}

using namespace ImGui;

void ProjectState::Render() const {
    auto const frame_count = GetCurrentContext()->FrameCount;

    auto dockspace_id = DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    if (frame_count == 1) Dock(&dockspace_id);

    // Draw non-window children.
    for (const auto *child : Core.Children) {
        if (!ProjectContext.IsWindow(child->Id) && child != &Core.Windows) child->Draw();
    }

    Core.Windows.Draw();

    if (frame_count == 1) {
        // Default focused windows.
        Core.Style.Focus();
        Core.Debug.Focus(); // not visible by default anymore

        FlowGrid.Audio.Graph.Focus();
        FlowGrid.Audio.Faust.Graphs.Focus();
        FlowGrid.Audio.Faust.Paramss.Focus();
    }
}
