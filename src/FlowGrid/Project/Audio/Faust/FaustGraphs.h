#pragma once

#include "Core/Container/Navigable.h"
#include "Core/Container/Vector.h"
#include "Core/Primitive/Flags.h"
#include "FaustGraphAction.h"
#include "FaustGraphStyle.h"

#include "FaustListener.h"

struct Node;
struct FaustGraphs;

struct FaustGraph : Component {
    FaustGraph(ComponentArgs &&);
    ~FaustGraph();

    float GetScale() const;

    void SaveBoxSvg(const fs::path &dir_path) const;
    void SetBox(Box);
    void ResetBox(); // Set to the box of the current root node.

    Prop(UInt, DspId);
    Prop(Navigable<ID>, NodeNavigationHistory);

    const FaustGraphs &Context;
    const FaustGraphStyle &Style;

    Box _Box;
    mutable std::unordered_map<ID, Node *> NodeByImGuiId;
    std::unique_ptr<Node> RootNode{};

private:
    void Render() const override;

    Node *Tree2Node(Box) const;
    Node *Tree2NodeInner(Box) const;
};

struct FaustGraphs : Component, Actionable<Action::Faust::Graph::Any>, Field::ChangeListener, FaustBoxChangeListener {
    FaustGraphs(ComponentArgs &&);
    ~FaustGraphs();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    FaustGraph *FindGraph(ID dsp_id) const;

    void OnFieldChanged() override;

    void OnFaustBoxChanged(ID, Box) override;
    void OnFaustBoxAdded(ID, Box) override;
    void OnFaustBoxRemoved(ID) override;

    void UpdateNodeImGuiIds() const;

    struct GraphSettings : Component {
        using Component::Component;

        Prop_(
            Flags, HoverFlags,
            "?Hovering over a node in the graph will display the selected information",
            {
                "ShowRect?Display the hovered node's bounding rectangle",
                "ShowType?Display the hovered node's box type",
                "ShowChannels?Display the hovered node's channel points and indices",
                "ShowChildChannels?Display the channel points and indices for each of the hovered node's children",
            },
            FaustGraphHoverFlags_None
        );
    };

    Prop(GraphSettings, Settings);
    Prop(FaustGraphStyle, Style);
    Prop(Vector<FaustGraph>, Graphs);

private:
    void Render() const override;
};
