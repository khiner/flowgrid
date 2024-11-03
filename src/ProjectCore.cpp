#include "ProjectCore.h"

#include "imgui_internal.h"
#include "implot.h"

#include "Core/UI/JsonTree.h"

#include "Project/ProjectContext.h"

using Style = flowgrid::Style;

ProjectCore::ProjectCore(ArgsT &&args) : ActionableComponent(std::move(args)) {
    Settings.RegisterWindow();
    Debug.RegisterWindow();
    Debug.StatePreview.RegisterWindow();
    Debug.StorePathUpdateFrequency.RegisterWindow();
    Debug.DebugLog.RegisterWindow();
    Debug.StackTool.RegisterWindow();
    Debug.Metrics.RegisterWindow();
    Style.RegisterWindow();
    Demo.RegisterWindow();
    Info.RegisterWindow();
}

void ProjectCore::Apply(const ActionType &action) const {
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
                const auto &colors = Style.ImGui.Colors;
                // todo enum types instead of raw int keys
                switch (a.id) {
                    case 0: return colors.Set(Style::ImGuiStyle::ColorsDark);
                    case 1: return colors.Set(Style::ImGuiStyle::ColorsLight);
                    case 2: return colors.Set(Style::ImGuiStyle::ColorsClassic);
                }
            },
            [this](const Action::Style::SetImPlotColorPreset &a) {
                const auto &style = Style.ImPlot;
                const auto &colors = style.Colors;
                switch (a.id) {
                    case 0:
                        colors.Set(Style::ImPlotStyle::ColorsAuto);
                        return style.MinorAlpha.Set(0.25f);
                    case 1:
                        colors.Set(Style::ImPlotStyle::ColorsDark);
                        return style.MinorAlpha.Set(0.25f);
                    case 2:
                        colors.Set(Style::ImPlotStyle::ColorsLight);
                        return style.MinorAlpha.Set(1);
                    case 3:
                        colors.Set(Style::ImPlotStyle::ColorsClassic);
                        return style.MinorAlpha.Set(0.5f);
                }
            },
            [this](const Action::Style::SetProjectColorPreset &a) {
                const auto &colors = Style.Project.Colors;
                switch (a.id) {
                    case 0: return colors.Set(ProjectStyle::ColorsDark);
                    case 1: return colors.Set(ProjectStyle::ColorsLight);
                    case 2: return colors.Set(ProjectStyle::ColorsClassic);
                }
            },
        },
        action
    );
}

bool ProjectCore::CanApply(const ActionType &action) const {
    return std::visit(
        Match{
            [](auto &&) { return true; },
        },
        action
    );
}

void ProjectCore::Debug::StorePathUpdateFrequency::Render() const {
    ProjectContext.RenderStorePathChangeFrequency();
}

using namespace ImGui;

// todo only overriding to draw self in addition to children - refactor to avoid this
void ProjectCore::Debug::DrawWindowsMenu() const {
    const auto &item = ProjectContext.DrawMenuItem;
    if (BeginMenu(Name.c_str())) {
        item(*this);
        item(StatePreview);
        item(StorePathUpdateFrequency);
        item(DebugLog);
        item(StackTool);
        item(Metrics);
        EndMenu();
    }
}

void ProjectCore::Debug::DebugLog::Render() const {
    ShowDebugLogWindow();
}
void ProjectCore::Debug::StackTool::Render() const {
    ShowIDStackToolWindow();
}

void ProjectCore::Debug::Metrics::ImGuiMetrics::Render() const { ImGui::ShowMetricsWindow(); }
void ProjectCore::Debug::Metrics::ImPlotMetrics::Render() const { ImPlot::ShowMetricsWindow(); }

void ProjectCore::Debug::OnComponentChanged() {
    if (AutoSelect.IsChanged()) {
        WindowFlags = AutoSelect ? ImGuiWindowFlags_NoScrollWithMouse : ImGuiWindowFlags_None;
    }
}

void ProjectCore::RenderDebug() const {
    const bool auto_select = Debug.AutoSelect;
    if (auto_select) BeginDisabled();
    RenderValueTree(Debug::LabelModeType(int(Debug.LabelMode)) == Debug::LabelModeType::Annotated, auto_select);
    if (auto_select) EndDisabled();
}

void ProjectCore::Debug::StatePreview::Render() const {
    Format.Draw();
    Raw.Draw();

    Separator();

    json project_json = ProjectContext.GetProjectJson(ProjectFormat(int(Format)));
    if (Raw) {
        TextUnformatted(project_json.dump(4));
    } else {
        SetNextItemOpen(true);
        flowgrid::JsonTree("", std::move(project_json));
    }
}

void ProjectCore::Debug::Metrics::Render() const {
    RenderTabs();
}

void ProjectCore::Debug::Metrics::ProjectMetrics::Render() const {
    ProjectContext.RenderMetrics();
}