#include "FaustGraphStyle.h"

#include "imgui.h"

FaustGraphStyle::FaustGraphStyle(ArgsT &&args) : ActionableComponent(std::move(args)) {
    ColorsDark();
    LayoutFlowGrid();
}

void FaustGraphStyle::Apply(const ActionType &action) const {
    Visit(
        action,
        [this](const Action::Faust::GraphStyle::ApplyColorPreset &a) {
            switch (a.id) {
                case 0: return ColorsDark();
                case 1: return ColorsLight();
                case 2: return ColorsClassic();
                case 3: return ColorsFaust();
            }
        },
        [this](const Action::Faust::GraphStyle::ApplyLayoutPreset &a) {
            switch (a.id) {
                case 0: return LayoutFlowGrid();
                case 1: return LayoutFaust();
            }
        },
    );
}

bool FaustGraphStyle::CanApply(const ActionType &) const { return true; }

const char *FaustGraphStyle::GetColorName(FlowGridGraphCol idx) {
    switch (idx) {
        case FlowGridGraphCol_Bg: return "Background";
        case FlowGridGraphCol_Text: return "Text";
        case FlowGridGraphCol_DecorateStroke: return "DecorateStroke";
        case FlowGridGraphCol_GroupStroke: return "GroupStroke";
        case FlowGridGraphCol_Line: return "Line";
        case FlowGridGraphCol_Link: return "Link";
        case FlowGridGraphCol_Inverter: return "Inverter";
        case FlowGridGraphCol_OrientationMark: return "OrientationMark";
        case FlowGridGraphCol_Normal: return "Normal";
        case FlowGridGraphCol_Ui: return "Ui";
        case FlowGridGraphCol_Slot: return "Slot";
        case FlowGridGraphCol_Number: return "Number";
        default: return "Unknown";
    }
}

void FaustGraphStyle::ColorsDark() const {
    Colors.Set(
        {
            {FlowGridGraphCol_Bg, {0.06, 0.06, 0.06, 0.94}},
            {FlowGridGraphCol_Text, {1, 1, 1, 1}},
            {FlowGridGraphCol_DecorateStroke, {0.43, 0.43, 0.5, 0.5}},
            {FlowGridGraphCol_GroupStroke, {0.43, 0.43, 0.5, 0.5}},
            {FlowGridGraphCol_Line, {0.61, 0.61, 0.61, 1}},
            {FlowGridGraphCol_Link, {0.26, 0.59, 0.98, 0.4}},
            {FlowGridGraphCol_Inverter, {1, 1, 1, 1}},
            {FlowGridGraphCol_OrientationMark, {1, 1, 1, 1}},
            // Box fills
            {FlowGridGraphCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridGraphCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridGraphCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridGraphCol_Number, {0.96, 0.28, 0, 1}},
        }
    );
}
void FaustGraphStyle::ColorsClassic() const {
    Colors.Set(
        {
            {FlowGridGraphCol_Bg, {0, 0, 0, 0.85}},
            {FlowGridGraphCol_Text, {0.9, 0.9, 0.9, 1}},
            {FlowGridGraphCol_DecorateStroke, {0.5, 0.5, 0.5, 0.5}},
            {FlowGridGraphCol_GroupStroke, {0.5, 0.5, 0.5, 0.5}},
            {FlowGridGraphCol_Line, {1, 1, 1, 1}},
            {FlowGridGraphCol_Link, {0.35, 0.4, 0.61, 0.62}},
            {FlowGridGraphCol_Inverter, {0.9, 0.9, 0.9, 1}},
            {FlowGridGraphCol_OrientationMark, {0.9, 0.9, 0.9, 1}},
            // Box fills
            {FlowGridGraphCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridGraphCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridGraphCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridGraphCol_Number, {0.96, 0.28, 0, 1}},
        }
    );
}
void FaustGraphStyle::ColorsLight() const {
    Colors.Set(
        {
            {FlowGridGraphCol_Bg, {0.94, 0.94, 0.94, 1}},
            {FlowGridGraphCol_Text, {0, 0, 0, 1}},
            {FlowGridGraphCol_DecorateStroke, {0, 0, 0, 0.3}},
            {FlowGridGraphCol_GroupStroke, {0, 0, 0, 0.3}},
            {FlowGridGraphCol_Line, {0.39, 0.39, 0.39, 1}},
            {FlowGridGraphCol_Link, {0.26, 0.59, 0.98, 0.4}},
            {FlowGridGraphCol_Inverter, {0, 0, 0, 1}},
            {FlowGridGraphCol_OrientationMark, {0, 0, 0, 1}},
            // Box fills
            {FlowGridGraphCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridGraphCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridGraphCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridGraphCol_Number, {0.96, 0.28, 0, 1}},
        }
    );
}
void FaustGraphStyle::ColorsFaust() const {
    Colors.Set(
        {
            {FlowGridGraphCol_Bg, {1, 1, 1, 1}},
            {FlowGridGraphCol_Text, {1, 1, 1, 1}},
            {FlowGridGraphCol_DecorateStroke, {0.2, 0.2, 0.2, 1}},
            {FlowGridGraphCol_GroupStroke, {0.2, 0.2, 0.2, 1}},
            {FlowGridGraphCol_Line, {0, 0, 0, 1}},
            {FlowGridGraphCol_Link, {0, 0.2, 0.4, 1}},
            {FlowGridGraphCol_Inverter, {0, 0, 0, 1}},
            {FlowGridGraphCol_OrientationMark, {0, 0, 0, 1}},
            // Box fills
            {FlowGridGraphCol_Normal, {0.29, 0.44, 0.63, 1}},
            {FlowGridGraphCol_Ui, {0.28, 0.47, 0.51, 1}},
            {FlowGridGraphCol_Slot, {0.28, 0.58, 0.37, 1}},
            {FlowGridGraphCol_Number, {0.96, 0.28, 0, 1}},
        }
    );
}

void FaustGraphStyle::LayoutFlowGrid() const {
    SequentialConnectionZigzag.Set(false);
    OrientationMark.Set(false);
    DecorateRootNode.Set(true);
    DecorateMargin.Set({10, 10});
    DecoratePadding.Set({10, 10});
    DecorateLineWidth.Set(1);
    DecorateCornerRadius.Set(0);
    GroupMargin.Set({8, 8});
    GroupPadding.Set({8, 8});
    GroupLineWidth.Set(2);
    GroupCornerRadius.Set(5);
    BoxCornerRadius.Set(4);
    BinaryHorizontalGapRatio.Set(0.25);
    WireThickness.Set(1);
    WireGap.Set(16);
    NodeMargin.Set({8, 8});
    NodePadding.Set({8, 0});
    NodeMinSize.Set({48, 48});
    ArrowSize.Set({3, 2});
    InverterRadius.Set(3);
}

void FaustGraphStyle::LayoutFaust() const {
    SequentialConnectionZigzag.Set(true);
    OrientationMark.Set(true);
    DecorateRootNode.Set(true);
    DecorateMargin.Set({10, 10});
    DecoratePadding.Set({10, 10});
    DecorateLineWidth.Set(1);
    DecorateCornerRadius.Set(0);
    GroupMargin.Set({10, 10});
    GroupPadding.Set({10, 10});
    GroupLineWidth.Set(1);
    GroupCornerRadius.Set(0);
    BoxCornerRadius.Set(0);
    BinaryHorizontalGapRatio.Set(0.25f);
    WireThickness.Set(1);
    WireGap.Set(16);
    NodeMargin.Set({8, 8});
    NodePadding.Set({8, 0});
    NodeMinSize.Set({48, 48});
    ArrowSize.Set({3, 2});
    InverterRadius.Set(3);
}

using namespace ImGui;

void FaustGraphStyle::Render() const {
    if (BeginTabBar(ImGuiLabel.c_str(), ImGuiTabBarFlags_None)) {
        if (BeginTabItem("Layout")) {
            static int graph_layout_idx = -1;
            if (Combo("Preset", &graph_layout_idx, "FlowGrid\0Faust\0")) Q(Action::Faust::GraphStyle::ApplyLayoutPreset{graph_layout_idx});

            FoldComplexity.Draw();
            const bool scale_fill = ScaleFillHeight;
            ScaleFillHeight.Draw();
            if (scale_fill) BeginDisabled();
            Scale.Draw();
            if (scale_fill) {
                SameLine();
                TextUnformatted(std::format("Uncheck '{}' to manually edit graph scale.", ScaleFillHeight.Name).c_str());
                EndDisabled();
            }
            Direction.Draw();
            OrientationMark.Draw();
            if (OrientationMark) {
                SameLine();
                SetNextItemWidth(GetContentRegionAvail().x * 0.5f);
                OrientationMarkRadius.Draw();
            }
            RouteFrame.Draw();
            SequentialConnectionZigzag.Draw();
            Separator();
            const bool decorate_folded = DecorateRootNode;
            DecorateRootNode.Draw();
            if (!decorate_folded) BeginDisabled();
            DecorateMargin.Draw();
            DecoratePadding.Draw();
            DecorateLineWidth.Draw();
            DecorateCornerRadius.Draw();
            if (!decorate_folded) EndDisabled();
            Separator();
            GroupMargin.Draw();
            GroupPadding.Draw();
            GroupLineWidth.Draw();
            GroupCornerRadius.Draw();
            Separator();
            NodeMargin.Draw();
            NodePadding.Draw();
            NodeMinSize.Draw();
            BoxCornerRadius.Draw();
            BinaryHorizontalGapRatio.Draw();
            WireGap.Draw();
            WireThickness.Draw();
            ArrowSize.Draw();
            InverterRadius.Draw();
            EndTabItem();
        }
        if (BeginTabItem(Colors.ImGuiLabel.c_str())) {
            static int graph_colors_idx = -1;
            if (Combo("Preset", &graph_colors_idx, "Dark\0Light\0Classic\0Faust\0")) Q(Action::Faust::GraphStyle::ApplyColorPreset{graph_colors_idx});

            Colors.Draw();
            EndTabItem();
        }
        EndTabBar();
    }
}
