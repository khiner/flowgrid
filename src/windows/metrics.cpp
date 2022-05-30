#include "../context.h"
#include "../imgui_helpers.h"

using namespace ImGui;

namespace FlowGrid {

// json diff conforms to the [JSON patch](http://jsonpatch.com/) spec.
// TODO deserialize into a `Patch` struct
void ShowStateDiffMetrics(const json &diff) {
    for (size_t i = 0; i < diff.size(); i++) {
        if (TreeNode(std::to_string(i).c_str())) {
            const auto &jd = diff[i];
            const std::string &path = jd["path"];
            const std::string &op = jd["op"];
            const std::string &value = jd["value"].dump();
            BulletText("Path: %s", path.c_str());
            BulletText("Op: %s", op.c_str());
            BulletText("Value:\n%s", value.c_str());

            TreePop();
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
        ShowStateDiffMetrics(diff.forward);
        TreePop();
    }
    if (TreeNode("Reverse diff")) {
        ShowStateDiffMetrics(diff.reverse);
        TreePop();
    }

    // TODO add https://github.com/fmtlib/fmt and use e.g.
    //    std::format("{:%Y/%m/%d %T}", tSysTime);
    BulletText("Nanos: %llu", diff.system_time.time_since_epoch().count());
    TreePop();
}

void ShowMetrics() {
    if (c.diffs.empty()) BeginDisabled();
    if (TreeNode("Diffs", "Diffs (%lu)", c.diffs.size())) {
        for (size_t i = 0; i < c.diffs.size(); i++) {
            if (TreeNode(std::to_string(i).c_str())) {
                ShowDiffMetrics(c.diffs[i]);
            }
        }
        TreePop();
    }
    if (c.diffs.empty()) EndDisabled();

    if (TreeNode("Actions")) {
        Text("Action variant size: %lu bytes", sizeof(Action));
        SameLine();
        HelpMarker("All actions are internally stored in an `std::variant`, which must be large enough to hold its largest type. "
                   "Thus, it's important to keep action data small.");

        TreePop();
    }
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
