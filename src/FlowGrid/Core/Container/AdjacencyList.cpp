#include "AdjacencyList.h"

#include "Core/Store/Store.h"

#include "imgui.h"

void AdjacencyList::Connect(ID source, ID destination) const {
    if (IsConnected(source, destination)) return;

    store::AddIdPair(Path, {source, destination});
}

void AdjacencyList::Disconnect(ID source, ID destination) const {
    store::EraseIdPair(Path, {source, destination});
}

void AdjacencyList::ToggleConnection(ID source, ID destination) const {
    if (IsConnected(source, destination)) Disconnect(source, destination);
    else Connect(source, destination);
}

bool AdjacencyList::IsConnected(ID source, ID destination) const {
    return store::HasIdPair(Path, {source, destination});
}

using namespace ImGui;

void AdjacencyList::RenderValueTree(bool annotate, bool auto_select) const {
    Field::RenderValueTree(annotate, auto_select);

    if (!store::IdPairCount(Path)) {
        TextUnformatted(std::format("{} (empty)", Name).c_str());
        return;
    }

    if (TreeNode(Name)) {
        const auto id_pairs = store::IdPairs(Path);
        Count i = 0;
        for (const auto &id_pair : id_pairs) {
            // todo if annotated, show node labels
            const std::string &label = std::format("{} -> {}", id_pair.first, id_pair.second);
            TreeNode(to_string(i++), false, label.c_str());
        }
        TreePop();
    }
}
