#include "ProjectState.h"

#include "imgui.h"

#include "Project/ProjectContext.h"

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

void ProjectState::FocusDefault() const {
    for (const auto *child : Children) child->FocusDefault();
}

void ProjectState::Render() const {
    auto dockspace_id = ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    if (FrameCount() == 1) Dock(&dockspace_id);

    const auto &windows = Core.Windows;
    for (const auto *child : Core.Children) {
        if (!windows.IsWindow(child->Id) && child != &windows) child->Draw();
    }
    windows.Draw();

    if (FrameCount() == 1) FocusDefault(); // todo default focus no longer working
}
