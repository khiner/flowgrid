#pragma once

#include <stack>

#include "Core/Primitive/Flags.h"
#include "FaustGraphAction.h"
#include "FaustGraphStyle.h"

#include "FaustListener.h"

struct Node;

struct FaustGraphs : Component, Actionable<Action::Faust::Graph::Any>, Field::ChangeListener, FaustBoxChangeListener {
    FaustGraphs(ComponentArgs &&);
    ~FaustGraphs();

    void Apply(const ActionType &) const override;
    bool CanApply(const ActionType &) const override;

    void OnFieldChanged() override;

    void OnFaustBoxChanged(ID, Box) override;
    void OnFaustBoxAdded(ID, Box) override;
    void OnFaustBoxRemoved(ID) override;

    float GetScale() const;
    void UpdateNodeImGuiIds() const;

    struct GraphSettings : Component {
        using Component::Component;

        Prop_(Flags, HoverFlags, "?Hovering over a node in the graph will display the selected information", {"ShowRect?Display the hovered node's bounding rectangle", "ShowType?Display the hovered node's box type", "ShowChannels?Display the hovered node's channel points and indices", "ShowChildChannels?Display the channel points and indices for each of the hovered node's children"}, FaustGraphHoverFlags_None);
    };

    Prop(GraphSettings, Settings);
    Prop(FaustGraphStyle, Style);

    mutable std::stack<Node *> FocusedNodeStack; // TODO public mutable is temporary. Replace with navigation actions.

private:
    void Render() const override;

    void SaveBoxSvg(const fs::path &dir_path) const;
    void OnFaustBoxChangedInner(Box);

    Node *Tree2Node(Box) const;
    Node *Tree2NodeInner(Box) const;

    std::unique_ptr<Node> RootNode{};
    // std::unordered_map<ID, Box> BoxById;
};
