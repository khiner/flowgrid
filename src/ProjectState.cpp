#include "ProjectState.h"

#include "imgui_internal.h"
#include "implot.h"

#include "Core/UI/JsonTree.h"

#include "Project/ProjectContext.h"

using namespace FlowGrid;

ProjectState::ProjectState(Store &store, ActionableProducer::EnqueueFn q, const ::ProjectContext &project_context)
    : Component(store, "ProjectState", PrimitiveQ, project_context, Style), ActionableProducer(std::move(q)) {
    Windows.SetWindowComponents({
        Audio.Graph,
        Audio.Graph.Connections,
        Audio.Style,
        Settings,
        Audio.Faust.FaustDsps,
        Audio.Faust.Logs,
        Audio.Faust.Graphs,
        Audio.Faust.Paramss,
        Debug,
        Debug.StatePreview,
        Debug.StorePathUpdateFrequency,
        Debug.DebugLog,
        Debug.StackTool,
        Debug.Metrics,
        Style,
        Demo,
        Info,
    });
}

ProjectState::~ProjectState() = default;

void ProjectState::Apply(const ActionType &action) const {
    std::visit(
        Match{
            [this](const Action::Windows::ToggleVisible &a) { Windows.ToggleVisible(a.component_id); },
            [this](const Action::Windows::ToggleDebug &a) {
                const bool toggling_on = !Windows.VisibleComponents.Contains(a.component_id);
                Windows.ToggleVisible(a.component_id);
                if (!toggling_on) return;

                auto *debug_component = static_cast<DebugComponent *>(Component::ById.at(a.component_id));
                if (auto *window = debug_component->FindDockWindow()) {
                    auto docknode_id = window->DockId;
                    auto debug_node_id = ImGui::DockBuilderSplitNode(docknode_id, ImGuiDir_Right, debug_component->SplitRatio, nullptr, &docknode_id);
                    debug_component->Dock(debug_node_id);
                }
            },
            [this](const Action::Style::SetImGuiColorPreset &a) {
                // todo enum types instead of raw int keys
                switch (a.id) {
                    case 0: return Style.ImGui.Colors.Set(Style::ImGuiStyle::ColorsDark);
                    case 1: return Style.ImGui.Colors.Set(Style::ImGuiStyle::ColorsLight);
                    case 2: return Style.ImGui.Colors.Set(Style::ImGuiStyle::ColorsClassic);
                }
            },
            [this](const Action::Style::SetImPlotColorPreset &a) {
                switch (a.id) {
                    case 0:
                        Style.ImPlot.Colors.Set(Style::ImPlotStyle::ColorsAuto);
                        return Style.ImPlot.MinorAlpha.Set(0.25f);
                    case 1:
                        Style.ImPlot.Colors.Set(Style::ImPlotStyle::ColorsDark);
                        return Style.ImPlot.MinorAlpha.Set(0.25f);
                    case 2:
                        Style.ImPlot.Colors.Set(Style::ImPlotStyle::ColorsLight);
                        return Style.ImPlot.MinorAlpha.Set(1);
                    case 3:
                        Style.ImPlot.Colors.Set(Style::ImPlotStyle::ColorsClassic);
                        return Style.ImPlot.MinorAlpha.Set(0.5f);
                }
            },
            [this](const Action::Style::SetFlowGridColorPreset &a) {
                switch (a.id) {
                    case 0: return Style.FlowGrid.Colors.Set(FlowGridStyle::ColorsDark);
                    case 1: return Style.FlowGrid.Colors.Set(FlowGridStyle::ColorsLight);
                    case 2: return Style.FlowGrid.Colors.Set(FlowGridStyle::ColorsClassic);
                }
            },
            [](const Action::TextBuffer::Any &a) {
                const auto *buffer = Component::ById.at(a.GetComponentId());
                static_cast<const TextBuffer *>(buffer)->Apply(a);
            },
            /* Audio */
            [this](const Action::AudioGraph::Any &a) { Audio.Graph.Apply(a); },
            [this](const Action::Faust::DSP::Create &) { Audio.Faust.FaustDsps.EmplaceBack(FaustDspPathSegment); },
            [this](const Action::Faust::DSP::Delete &a) { Audio.Faust.FaustDsps.EraseId(a.id); },
            [this](const Action::Faust::Graph::Any &a) { Audio.Faust.Graphs.Apply(a); },
            [this](const Action::Faust::GraphStyle::ApplyColorPreset &a) {
                const auto &colors = Audio.Faust.Graphs.Style.Colors;
                switch (a.id) {
                    case 0: return colors.Set(FaustGraphStyle::ColorsDark);
                    case 1: return colors.Set(FaustGraphStyle::ColorsLight);
                    case 2: return colors.Set(FaustGraphStyle::ColorsClassic);
                    case 3: return colors.Set(FaustGraphStyle::ColorsFaust);
                }
            },
            [this](const Action::Faust::GraphStyle::ApplyLayoutPreset &a) {
                const auto &style = Audio.Faust.Graphs.Style;
                switch (a.id) {
                    case 0: return style.LayoutFlowGrid();
                    case 1: return style.LayoutFaust();
                }
            },
            [](const Action::Core::Any &&) {}, // All other actions are project actions.

        },
        action
    );
}

bool ProjectState::CanApply(const ActionType &action) const {
    return std::visit(
        Match{
            [this](const Action::AudioGraph::Any &a) { return Audio.Graph.CanApply(a); },
            [this](const Action::Faust::Graph::Any &a) { return Audio.Faust.Graphs.CanApply(a); },
            [](auto &&) { return true; }, // All other actions are always allowed.
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

        Audio.Graph.Dock(audio_node_id);
        Audio.Graph.Connections.Dock(audio_node_id);
        Audio.Style.Dock(audio_node_id);

        Audio.Faust.FaustDsps.Dock(dockspace_id);
        Audio.Faust.Graphs.Dock(faust_graph_node_id);
        Audio.Faust.Paramss.Dock(faust_tools_node_id);
        Audio.Faust.Logs.Dock(faust_tools_node_id);

        Debug.Dock(debug_node_id);
        Debug.StatePreview.Dock(debug_node_id);
        Debug.StorePathUpdateFrequency.Dock(debug_node_id);
        Debug.DebugLog.Dock(debug_node_id);
        Debug.StackTool.Dock(debug_node_id);
        Debug.Metrics.Dock(metrics_node_id);

        Style.Dock(utilities_node_id);
        Demo.Dock(utilities_node_id);

        Info.Dock(info_node_id);
        Settings.Dock(settings_node_id);
    }

    // Draw non-window children.
    for (const auto *child : Children) {
        if (!Windows.IsWindow(child->Id) && child != &Windows) child->Draw();
    }

    Windows.Draw();

    if (frame_count == 1) {
        // Default focused windows.
        Style.Focus();
        Audio.Graph.Focus();
        Audio.Faust.Graphs.Focus();
        Audio.Faust.Paramss.Focus();
        Debug.Focus(); // not visible by default anymore
    }
}

void ProjectState::Debug::StorePathUpdateFrequency::Render() const {
    ProjectContext.RenderStorePathChangeFrequency();
}

void ProjectState::Debug::DebugLog::Render() const {
    ShowDebugLogWindow();
}
void ProjectState::Debug::StackTool::Render() const {
    ShowIDStackToolWindow();
}

void ProjectState::Debug::Metrics::ImGuiMetrics::Render() const { ImGui::ShowMetricsWindow(); }
void ProjectState::Debug::Metrics::ImPlotMetrics::Render() const { ImPlot::ShowMetricsWindow(); }

void ProjectState::Debug::OnComponentChanged() {
    if (AutoSelect.IsChanged()) {
        WindowFlags = AutoSelect ? ImGuiWindowFlags_NoScrollWithMouse : ImGuiWindowFlags_None;
    }
}

void ProjectState::RenderDebug() const {
    const bool auto_select = Debug.AutoSelect;
    if (auto_select) BeginDisabled();
    RenderValueTree(Debug::LabelModeType(int(Debug.LabelMode)) == Debug::LabelModeType::Annotated, auto_select);
    if (auto_select) EndDisabled();
}

void ProjectState::Debug::StatePreview::Render() const {
    Format.Draw();
    Raw.Draw();

    Separator();

    json project_json = ProjectContext.GetProjectJson(ProjectFormat(int(Format)));
    if (Raw) {
        TextUnformatted(project_json.dump(4));
    } else {
        SetNextItemOpen(true);
        fg::JsonTree("", std::move(project_json));
    }
}

void ProjectState::Debug::Metrics::Render() const {
    RenderTabs();
}

void ProjectState::Debug::Metrics::FlowGridMetrics::Render() const {
    ProjectContext.RenderMetrics();
}
