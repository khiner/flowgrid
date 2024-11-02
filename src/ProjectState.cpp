#include "ProjectState.h"

#include "imgui_internal.h"
#include "implot.h"

#include "Project/ProjectContext.h"

using namespace flowgrid;

ProjectState::ProjectState(Store &store, ActionableProducer::Enqueue q, const ::ProjectContext &project_context)
    : Component(store, "ProjectState", project_context), ActionableProducer(std::move(q)) {
    Core.Windows.SetWindowComponents({
        Core.Settings,
        Core.Debug,
        Core.Debug.StatePreview,
        Core.Debug.StorePathUpdateFrequency,
        Core.Debug.DebugLog,
        Core.Debug.StackTool,
        Core.Debug.Metrics,
        Core.Style,
        Core.Demo,
        Core.Info,

        FlowGrid.Audio.Graph,
        FlowGrid.Audio.Graph.Connections,
        FlowGrid.Audio.Style,
        FlowGrid.Audio.Faust.FaustDsps,
        FlowGrid.Audio.Faust.Logs,
        FlowGrid.Audio.Faust.Graphs,
        FlowGrid.Audio.Faust.Paramss,
    });
}

ProjectState::~ProjectState() = default;

void ProjectState::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](Action::ProjectCore::Any &&a) { Core.Apply(std::move(a)); },
            [this](Action::FlowGrid::Any &&a) { FlowGrid.Apply(std::move(a)); },
        },
        action
    );
}

bool ProjectState::CanApply(const ActionType &action) const {
    return std::visit(
        Match{
            [this](Action::ProjectCore::Any &&a) { return Core.CanApply(std::move(a)); },
            [this](Action::FlowGrid::Any &&a) { return FlowGrid.CanApply(std::move(a)); },
        },
        action
    );
}

using namespace ImGui;

void ProjectState::Render() const {
    // Good initial layout setup example in this issue: https://github.com/ocornut/imgui/issues/3548
    auto dockspace_id = DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    int frame_count = GetCurrentContext()->FrameCount;
    if (frame_count == 1) {
        auto debug_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.3f, nullptr, &dockspace_id);
        auto metrics_node_id = DockBuilderSplitNode(debug_node_id, ImGuiDir_Right, 0.3f, nullptr, &debug_node_id);
        auto utilities_node_id = DockBuilderSplitNode(debug_node_id, ImGuiDir_Left, 0.3f, nullptr, &debug_node_id);

        auto info_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.2f, nullptr, &dockspace_id);
        auto settings_node_id = DockBuilderSplitNode(info_node_id, ImGuiDir_Down, 0.25f, nullptr, &info_node_id);

        auto audio_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, nullptr, &dockspace_id);
        auto faust_tools_node_id = DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.5f, nullptr, &dockspace_id);
        auto faust_graph_node_id = DockBuilderSplitNode(faust_tools_node_id, ImGuiDir_Left, 0.5f, nullptr, &faust_tools_node_id);
        DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.5f, nullptr, &dockspace_id); // text editor

        FlowGrid.Audio.Graph.Dock(audio_node_id);
        FlowGrid.Audio.Graph.Connections.Dock(audio_node_id);
        FlowGrid.Audio.Style.Dock(audio_node_id);

        FlowGrid.Audio.Faust.FaustDsps.Dock(dockspace_id);
        FlowGrid.Audio.Faust.Graphs.Dock(faust_graph_node_id);
        FlowGrid.Audio.Faust.Paramss.Dock(faust_tools_node_id);
        FlowGrid.Audio.Faust.Logs.Dock(faust_tools_node_id);

        Core.Debug.Dock(debug_node_id);
        Core.Debug.StatePreview.Dock(debug_node_id);
        Core.Debug.StorePathUpdateFrequency.Dock(debug_node_id);
        Core.Debug.DebugLog.Dock(debug_node_id);
        Core.Debug.StackTool.Dock(debug_node_id);
        Core.Debug.Metrics.Dock(metrics_node_id);

        Core.Style.Dock(utilities_node_id);
        Core.Demo.Dock(utilities_node_id);

        Core.Info.Dock(info_node_id);
        Core.Settings.Dock(settings_node_id);
    }

    // Draw non-window children.
    for (const auto *child : Core.Children) {
        if (!Core.Windows.IsWindow(child->Id) && child != &Core.Windows) child->Draw();
    }

    Core.Windows.Draw();

    if (frame_count == 1) {
        // Default focused windows.
        Core.Style.Focus();
        FlowGrid.Audio.Graph.Focus();
        FlowGrid.Audio.Faust.Graphs.Focus();
        FlowGrid.Audio.Faust.Paramss.Focus();
        Core.Debug.Focus(); // not visible by default anymore
    }
}
