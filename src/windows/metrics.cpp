#include "../context.h"
#include "../imgui_helpers.h"

using namespace ImGui;

namespace FlowGrid {

void ShowJsonPatchOpMetrics(const JsonPatchOp &patch_op) {
    BulletText("Path: %s", patch_op.path.c_str());
    BulletText("Op: %s", json(patch_op.op).dump().c_str());
    if (patch_op.value.has_value()) {
        BulletText("Value: %s", patch_op.value.value().dump().c_str());
    }
    if (patch_op.from.has_value()) {
        BulletText("From: %s", patch_op.from.value().c_str());
    }
}

void ShowJsonPatchMetrics(const JsonPatch &patch) {
    if (patch.size() == 1) {
        ShowJsonPatchOpMetrics(patch[0]);
    } else {
        for (size_t i = 0; i < patch.size(); i++) {
            if (TreeNodeEx(std::to_string(i).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                ShowJsonPatchOpMetrics(patch[i]);
                TreePop();
            }
        }
    }
}

void ShowDiffMetrics(const BidirectionalStateDiff &diff) {
    if (diff.action_names.size() == 1) {
        BulletText("Action name: %s", (*diff.action_names.begin()).c_str());
    } else {
        if (TreeNode("Actions", "%lu actions", diff.action_names.size())) {
            for (const auto &action_name: diff.action_names) {
                BulletText("%s", action_name.c_str());
            }
            TreePop();
        }
    }
    if (TreeNode("Forward diff")) {
        ShowJsonPatchMetrics(diff.forward_patch);
        TreePop();
    }
    if (TreeNode("Reverse diff")) {
        ShowJsonPatchMetrics(diff.reverse_patch);
        TreePop();
    }

    // TODO add https://github.com/fmtlib/fmt and use e.g.
    //    std::format("{:%Y/%m/%d %T}", tSysTime);
    BulletText("Nanos: %llu", diff.system_time.time_since_epoch().count());
    TreePop();
}

void ShowMetrics() {
    const bool has_diffs = !c.diffs.empty();
    if (!has_diffs) BeginDisabled();
    if (TreeNodeEx("Diffs", ImGuiTreeNodeFlags_DefaultOpen, "Diffs (Count: %lu, Current index: %d)", c.diffs.size(), c.current_action_index)) {
        for (size_t i = 0; i < c.diffs.size(); i++) {
            if (TreeNodeEx(std::to_string(i).c_str(), int(i) == c.current_action_index ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None)) {
                ShowDiffMetrics(c.diffs[i]);
            }
        }
        TreePop();
    }
    if (!has_diffs) EndDisabled();

    Text("Action variant size: %lu bytes", sizeof(Action));
    SameLine();
    HelpMarker("All actions are internally stored in an `std::variant`, which must be large enough to hold its largest type. "
               "Thus, it's important to keep action data small.");
}

}

void State::Metrics::draw() {
    if (BeginTabBar("##tabs")) {
        if (BeginTabItem("FlowGrid")) {
            FlowGrid::ShowMetrics();
            EndTabItem();
        }
        if (BeginTabItem("ImGui")) {
            ShowMetrics();
            EndTabItem();
        }
        if (BeginTabItem("ImPlot")) {
            ImPlot::ShowMetrics();
            EndTabItem();
        }
        EndTabBar();
    }
}
