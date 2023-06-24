#pragma once

#include "App/Style/Colors.h"
#include "Core/Container/Vec2.h"
#include "Core/Primitive/Bool.h"
#include "Core/Primitive/Enum.h"
#include "Core/Primitive/Flags.h"
#include "Core/Primitive/Float.h"
#include "Core/Primitive/String.h"
#include "FaustGraphAction.h"
#include "UI/Styling.h"

enum FaustGraphHoverFlags_ {
    FaustGraphHoverFlags_None = 0,
    FaustGraphHoverFlags_ShowRect = 1 << 0,
    FaustGraphHoverFlags_ShowType = 1 << 1,
    FaustGraphHoverFlags_ShowChannels = 1 << 2,
    FaustGraphHoverFlags_ShowChildChannels = 1 << 3,
};
using FaustGraphHoverFlags = int;

enum FlowGridGraphCol_ {
    FlowGridGraphCol_Bg, // ImGuiCol_WindowBg
    FlowGridGraphCol_Text, // ImGuiCol_Text
    FlowGridGraphCol_DecorateStroke, // ImGuiCol_Border
    FlowGridGraphCol_GroupStroke, // ImGuiCol_Border
    FlowGridGraphCol_Line, // ImGuiCol_PlotLines
    FlowGridGraphCol_Link, // ImGuiCol_Button
    FlowGridGraphCol_Inverter, // ImGuiCol_Text
    FlowGridGraphCol_OrientationMark, // ImGuiCol_Text
    // Box fill colors of various types. todo design these colors for Dark/Classic/Light profiles
    FlowGridGraphCol_Normal,
    FlowGridGraphCol_Ui,
    FlowGridGraphCol_Slot,
    FlowGridGraphCol_Number,

    FlowGridGraphCol_COUNT
};
using FlowGridGraphCol = int;

struct FaustGraph : Component, Actionable<Action::FaustGraph::Any> {
    FaustGraph(ComponentArgs &&args)
        : Component(
              std::move(args),
              Menu({
                  Menu("File", {Action::FaustGraph::ShowSaveSvgDialog::MenuItem}),
                  Menu("View", {Settings.HoverFlags}),
              })
          ) {}

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    struct Style : Component {
        Style(ComponentArgs &&);

        Prop_(
            UInt, FoldComplexity,
            "?Number of boxes within a graph before folding into a sub-graph.\n"
            "Setting to zero disables folding altogether, for a fully-expanded graph.",
            3, 0, 20
        );
        Prop_(Bool, ScaleFillHeight, "?Automatically scale to fill the full height of the graph window, keeping the same aspect ratio.");
        Prop(Float, Scale, 1, 0.1, 5);
        Prop(Enum, Direction, {"Left", "Right"}, Dir_Right);
        Prop(Bool, RouteFrame);
        Prop(Bool, SequentialConnectionZigzag); // `false` uses diagonal lines instead of zigzags instead of zigzags
        Prop(Bool, OrientationMark);
        Prop(Float, OrientationMarkRadius, 1.5, 0.5, 3);

        Prop(Bool, DecorateRootNode);
        Prop(Vec2Linked, DecorateMargin, {10, 10}, 0, 20);
        Prop(Vec2Linked, DecoratePadding, {10, 10}, 0, 20);
        Prop(Float, DecorateLineWidth, 1, 1, 4);
        Prop(Float, DecorateCornerRadius, 0, 0, 10);

        Prop(Vec2Linked, GroupMargin, {8, 8}, 0, 20);
        Prop(Vec2Linked, GroupPadding, {8, 8}, 0, 20);
        Prop(Float, GroupLineWidth, 2, 1, 4);
        Prop(Float, GroupCornerRadius, 5, 0, 10);

        Prop(Vec2Linked, NodeMargin, {8, 8}, 0, 20);
        Prop(Vec2Linked, NodePadding, {8, 0}, 0, 20, false); // todo padding y not actually used yet, since blocks already have a min-height determined by WireGap.

        Prop(Float, BoxCornerRadius, 4, 0, 10);
        Prop(Float, BinaryHorizontalGapRatio, 0.25, 0, 1);
        Prop(Float, WireWidth, 1, 0.5, 4);
        Prop(Float, WireGap, 16, 10, 20);
        Prop(Vec2, ArrowSize, {3, 2}, 1, 10);
        Prop(Float, InverterRadius, 3, 1, 5);

        Prop(Colors, Colors, FlowGridGraphCol_COUNT, GetColorName);

        void ColorsDark() const;
        void ColorsClassic() const;
        void ColorsLight() const;
        void ColorsFaust() const; // Color Faust graphs the same way Faust does when it renders to SVG.

        void LayoutFlowGrid() const;
        void LayoutFaust() const; // Layout Faust graphs the same way Faust does when it renders to SVG.

        static const char *GetColorName(FlowGridGraphCol idx);

    protected:
        void Render() const override;
    };

    struct GraphSettings : Component {
        using Component::Component;

        Prop_(Flags, HoverFlags, "?Hovering over a node in the graph will display the selected information", {"ShowRect?Display the hovered node's bounding rectangle", "ShowType?Display the hovered node's box type", "ShowChannels?Display the hovered node's channel points and indices", "ShowChildChannels?Display the channel points and indices for each of the hovered node's children"}, FaustGraphHoverFlags_None);
    };

    Prop(GraphSettings, Settings);
    Prop(Style, Style);

protected:
    void Render() const override;
};

extern const FaustGraph &faust_graph;
