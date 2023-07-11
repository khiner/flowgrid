#include "AdjacencyList.h"

#include <stack>

#include "imgui.h"

#include "Core/Store/Store.h"

void AdjacencyList::Connect(ID source, ID destination) const {
    if (IsConnected(source, destination)) return;

    RootStore.AddIdPair(Path, {source, destination});
}

void AdjacencyList::Disconnect(ID source, ID destination) const {
    RootStore.EraseIdPair(Path, {source, destination});
}

void AdjacencyList::ToggleConnection(ID source, ID destination) const {
    if (IsConnected(source, destination)) Disconnect(source, destination);
    else Connect(source, destination);
}

bool AdjacencyList::IsConnected(ID source, ID destination) const {
    return RootStore.HasIdPair(Path, {source, destination});
}

bool AdjacencyList::HasPath(ID from_id, ID to_id, const std::unordered_set<ID> &disabled) const {
    // Non-recursive depth-first search.
    const auto id_pairs = RootStore.IdPairs(Path);
    std::unordered_set<ID> visited;
    std::stack<ID> to_visit;
    to_visit.push(from_id);

    while (!to_visit.empty()) {
        ID current = to_visit.top();
        to_visit.pop();

        if (disabled.contains(current)) continue;
        if (current == to_id) return true;

        if (!visited.contains(current)) {
            visited.insert(current);

            for (const auto &pair : id_pairs) {
                if (pair.first == current) {
                    to_visit.push(pair.second);
                }
            }
        }
    }

    return false;
}

using namespace ImGui;

void AdjacencyList::RenderValueTree(bool annotate, bool auto_select) const {
    Field::RenderValueTree(annotate, auto_select);

    if (!RootStore.IdPairCount(Path)) {
        TextUnformatted(std::format("{} (empty)", Name).c_str());
        return;
    }

    if (TreeNode(Name)) {
        const auto id_pairs = RootStore.IdPairs(Path);
        Count i = 0;
        for (const auto &id_pair : id_pairs) {
            const ID source_id = id_pair.first;
            const ID destination_id = id_pair.second;
            const bool can_annotate = annotate && ById.contains(source_id) && ById.contains(destination_id);
            const std::string label = can_annotate ?
                std::format("{} -> {}", ById.at(source_id)->Name, ById.at(destination_id)->Name) :
                std::format("#{:08X} -> #{:08X}", source_id, destination_id);
            TreeNode(to_string(i++), false, label.c_str(), can_annotate);
        }
        TreePop();
    }
}
