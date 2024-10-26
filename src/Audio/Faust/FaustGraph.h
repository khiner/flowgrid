#pragma once

#include "Core/ActionProducerComponent.h"
#include "Core/Container/Navigable.h"
#include "FaustGraphAction.h"

class CTree;
typedef CTree *Box;

namespace flowgrid {
struct Node;
}

struct FaustGraphStyle;
struct FaustGraphSettings;

struct FaustGraph : ActionProducerComponent<Action::Combine<Action::Faust::Graph::Any, Navigable<ID>::ProducedActionType>> {
    FaustGraph(ArgsT &&, const FaustGraphStyle &, const FaustGraphSettings &);
    ~FaustGraph();

    float GetScale() const;

    void SaveBoxSvg(const fs::path &dir_path) const;
    void SetBox(Box);
    void ResetBox(); // Set to the box of the current root node.

    Prop(UInt, DspId);
    ProducerProp(Navigable<ID>, NodeNavigationHistory);

    const FaustGraphStyle &Style;
    const FaustGraphSettings &Settings;

    Box _Box;
    mutable std::unordered_map<ID, fg::Node *> NodeByImGuiId;
    std::unique_ptr<fg::Node> RootNode{};

private:
    void Render() const override;

    fg::Node *Tree2Node(Box) const;
    fg::Node *Tree2NodeInner(Box) const;
};
