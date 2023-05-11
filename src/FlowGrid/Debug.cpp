#include "Debug.h"

#include "imgui.h"
#include "implot.h"

#include "Store/StoreHistory.h"

using namespace ImGui;

void Debug::Render() const {}

void Debug::StorePathUpdateFrequency::Render() const {
    auto [labels, values] = History.StorePathUpdateFrequencyPlottable();
    if (labels.empty()) {
        Text("No state updates yet.");
        return;
    }

    if (ImPlot::BeginPlot("Path update frequency", {-1, float(labels.size()) * 30 + 60}, ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Number of updates", nullptr, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_Invert);

        // Hack to allow `SetupAxisTicks` without breaking on assert `n_ticks > 1`: Just add an empty label and only plot one value.
        // todo fix in ImPlot
        if (labels.size() == 1) labels.emplace_back("");

        // todo add an axis flag to exclude non-integer ticks
        // todo add an axis flag to show last tick
        ImPlot::SetupAxisTicks(ImAxis_Y1, 0, double(labels.size() - 1), int(labels.size()), labels.data(), false);
        static const char *ItemLabels[] = {"Committed updates", "Active updates"};
        const int item_count = !History.ActiveGesture.empty() ? 2 : 1;
        const int group_count = int(values.size()) / item_count;
        ImPlot::PlotBarGroups(ItemLabels, values.data(), item_count, group_count, 0.75, 0, ImPlotBarGroupsFlags_Horizontal | ImPlotBarGroupsFlags_Stacked);

        ImPlot::EndPlot();
    }
}

void Debug::DebugLog::Render() const {
    ShowDebugLogWindow();
}
void Debug::StackTool::Render() const {
    ShowStackToolWindow();
}

void Metrics::ImGuiMetrics::Render() const { ShowMetricsWindow(); }
void Metrics::ImPlotMetrics::Render() const { ImPlot::ShowMetricsWindow(); }

// #include "imgui_memory_editor.h"

// todo need to rethink this with the store system
// void Debug::StateMemoryEditor::Render() const {
//     static MemoryEditor memory_editor;
//     static bool first_render{true};
//     if (first_render) {
//         memory_editor.OptShowDataPreview = true;
//         //        memory_editor.WriteFn = ...; todo write_state_bytes action
//         first_render = false;
//     }

//     const void *mem_data{&s};
//     memory_editor.DrawContents(mem_data, sizeof(s));
// }
