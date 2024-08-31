#include "FaustParamGroup.h"

#include "FaustParam.h"

#include "FaustParamsStyle.h"

#include <imgui.h>

using namespace ImGui;
using namespace fg;

using enum FaustParamType;
using std::min, std::max;
using std::ranges::any_of, std::views::transform;

static auto Params(const FaustParamGroup &group) {
    return group.Children | transform([](const auto *child) { return dynamic_cast<const FaustParamBase *>(child); });
}

void FaustParamGroup::Render(float suggested_height, bool no_label) const {
    const char *label = no_label ? "" : Label.c_str();
    const auto &imgui_style = ImGui::GetStyle();
    const float frame_height = GetFrameHeight();
    const bool has_label = strlen(label) > 0;
    const float label_height = has_label ? CalcLabelHeight() : 0;

    if (has_label) TextUnformatted(label);

    if (Type == Type_TGroup) {
        const bool is_height_constrained = suggested_height != 0;
        // In addition to the group contents, account for the tab height and the space between the tabs and the content.
        const float group_height = max(0.f, is_height_constrained ? suggested_height - label_height : 0);
        const float item_height = max(0.f, group_height - frame_height - imgui_style.ItemSpacing.y);
        BeginTabBar(Label.c_str());
        for (const auto *child : Params(*this)) {
            if (BeginTabItem(child->Label.c_str())) {
                child->Render(item_height, true);
                EndTabItem();
            }
        }
        EndTabBar();
        return;
    }

    const float cell_padding = Type == Type_None ? 0 : 2 * imgui_style.CellPadding.y;
    const bool is_h = Type == Type_HGroup;
    float suggested_item_height = 0; // Including any label height, not including cell padding
    if (is_h) {
        const bool include_labels = !Style.HeaderTitles;
        suggested_item_height = std::ranges::max(
            Params(*this) | transform([include_labels](const auto *child) {
                return child->CalcHeight() + (include_labels ? child->CalcLabelHeight() : 0);
            })
        );
    }
    if (Type == Type_None) { // Root group (treated as a vertical group but not as a table)
        for (const auto *child : Params(*this)) child->Render(suggested_item_height);
        return;
    }

    if (BeginTable(ParamId.c_str(), is_h ? int(Children.size()) : 1, TableFlagsToImGui(Style.TableFlags))) {
        const float row_min_height = suggested_item_height + cell_padding;
        if (is_h) {
            ParamsWidthSizingPolicy policy = Style.WidthSizingPolicy;
            const bool allow_fixed_width_params =
                policy != ParamsWidthSizingPolicy_Balanced &&
                (policy == ParamsWidthSizingPolicy_StretchFlexibleOnly ||
                 (policy == ParamsWidthSizingPolicy_StretchToFill && any_of(Params(*this), [](const auto *child) { return child->IsWidthExpandable(); })));
            for (const auto *child : Params(*this)) {
                ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_None;
                if (allow_fixed_width_params && !child->IsWidthExpandable()) flags |= ImGuiTableColumnFlags_WidthFixed;
                TableSetupColumn(child->Label.c_str(), flags, child->CalcWidth(true));
            }
            if (Style.HeaderTitles) {
                // Custom headers (instead of `TableHeadersRow()`) to align column names.
                TableNextRow(ImGuiTableRowFlags_Headers);
                for (int column = 0; column < int(Children.size()); column++) {
                    TableSetColumnIndex(column);
                    const char *column_name = TableGetColumnName(column);
                    PushID(column);
                    const float header_x = CalcAlignedX(Style.AlignmentHorizontal, CalcTextSize(column_name).x, GetContentRegionAvail().x);
                    SetCursorPosX(GetCursorPosX() + max(0.f, header_x));
                    TableHeader(column_name);
                    PopID();
                }
            }
            TableNextRow(ImGuiTableRowFlags_None, row_min_height);
        }
        for (const auto *child : Params(*this)) {
            if (!is_h) TableNextRow(ImGuiTableRowFlags_None, row_min_height);
            TableNextColumn();
            TableSetBgColor(ImGuiTableBgTarget_RowBg0, GetColorU32(ImGuiCol_TitleBgActive, 0.1f));
            // const string child_label = child->Type == Type_Button || !is_h || !Style.HeaderTitles ? child->Label : "";
            child->Render(suggested_item_height);
        }
        EndTable();
    }
}
