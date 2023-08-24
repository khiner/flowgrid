#pragma once

#include "Core/Container/Navigable.h"

class CTree;
typedef CTree *Box;

struct Node;
struct FaustGraphStyle;
struct FaustGraphSettings;

struct FaustGraph : Component {
    FaustGraph(ComponentArgs &&, const FaustGraphStyle &, const FaustGraphSettings &);
    ~FaustGraph();

    float GetScale() const;
    std::optional<std::string> GetBoxInfo(u32 id) const;

    void SaveBoxSvg(const fs::path &dir_path) const;
    void SetBox(Box);
    void ResetBox(); // Set to the box of the current root node.

    Prop(UInt, DspId);
    Prop(Navigable<ID>, NodeNavigationHistory);

    const FaustGraphStyle &Style;
    const FaustGraphSettings &Settings;

    Box _Box;
    mutable std::unordered_map<ID, Node *> NodeByImGuiId;
    std::unique_ptr<Node> RootNode{};

private:
    void Render() const override;

    Node *Tree2Node(Box) const;
    Node *Tree2NodeInner(Box) const;
};
