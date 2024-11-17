#pragma once

#include "Core/ActionProducerComponent.h"
#include "Core/Container/Navigable.h"
#include "FaustGraphAction.h"

class CTreeBase;
typedef CTreeBase *Box;

namespace flowgrid {
struct Node;
}

struct FaustGraphStyle;
struct FaustGraphSettings;

struct FaustGraph : ActionProducerComponent<Action::Faust::Graph::Any> {
    FaustGraph(ArgsT &&, const FaustGraphStyle &, const FaustGraphSettings &);
    ~FaustGraph();

    float GetScale() const;

    void SaveBoxSvg(const fs::path &dir_path) const;
    void SetBox(Box);
    void ResetBox(); // Set to the box of the current root node.

    Prop(UInt, DspId);
    Prop(Navigable<u32>, NodeNavigationHistory);

    const FaustGraphStyle &Style;
    const FaustGraphSettings &Settings;

    Box _Box;
    mutable std::unordered_map<ID, flowgrid::Node *> NodeByImGuiId;
    std::unique_ptr<flowgrid::Node> RootNode{};

private:
    void Render() const override;

    flowgrid::Node *Tree2Node(Box) const;
    flowgrid::Node *Tree2NodeInner(Box) const;
};
