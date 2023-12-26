#include "AdjacencyList.h"

#include <stack>

#include "imgui.h"

#include "Core/Store/Store.h"

AdjacencyList::AdjacencyList(ArgsT &&args) : Container(std::move(args.Args)), ActionableProducer(std::move(args.Q)) {}

IdPairs AdjacencyList::Get() const { return RootStore.IdPairs(Path); }

void AdjacencyList::Add(IdPair &&id_pair) const {
    if (RootStore.HasIdPair(Path, id_pair)) return;

    RootStore.AddIdPair(Path, std::move(id_pair));
}

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

void AdjacencyList::DisconnectOutput(ID id) const {
    const auto id_pairs = Get();
    for (const auto &[source_id, destination_id] : id_pairs) {
        if (source_id == id || destination_id == id) Disconnect(source_id, destination_id);
    }
}

void AdjacencyList::Erase() const {
    RootStore.ClearIdPairs(Path);
}

bool AdjacencyList::IsConnected(ID source, ID destination) const {
    return RootStore.HasIdPair(Path, {source, destination});
}

bool AdjacencyList::HasPath(ID from_id, ID to_id) const {
    // Non-recursive depth-first search that handles cycles.
    const auto id_pairs = Get();
    std::unordered_set<ID> visited;
    std::stack<ID> to_visit;
    to_visit.push(from_id);

    while (!to_visit.empty()) {
        ID current = to_visit.top();
        to_visit.pop();

        if (current == to_id) return true;

        if (!visited.contains(current)) {
            visited.insert(current);

            for (const auto &[source_id, destination_id] : id_pairs) {
                if (source_id == current) to_visit.push(destination_id);
            }
        }
    }

    return false;
}

u32 AdjacencyList::SourceCount(ID destination) const {
    u32 count = 0;
    for (const auto &[source_id, destination_id] : Get()) {
        if (destination_id == destination) count++;
    }
    return count;
}

u32 AdjacencyList::DestinationCount(ID source) const {
    u32 count = 0;
    for (const auto &[source_id, destination_id] : Get()) {
        if (source_id == source) count++;
    }
    return count;
}

using namespace ImGui;

void AdjacencyList::RenderValueTree(bool annotate, bool auto_select) const {
    FlashUpdateRecencyBackground();

    if (!RootStore.IdPairCount(Path)) {
        TextUnformatted(std::format("{} (empty)", Name).c_str());
        return;
    }

    if (TreeNode(Name)) {
        u32 i = 0;
        for (const auto &[source_id, destination_id] : Get()) {
            const bool can_annotate = annotate && ById.contains(source_id) && ById.contains(destination_id);
            const std::string label = can_annotate ?
                std::format("{} -> {}", ById.at(source_id)->Name, ById.at(destination_id)->Name) :
                std::format("#{:08X} -> #{:08X}", source_id, destination_id);
            TreeNode(to_string(i++), false, label.c_str(), can_annotate);
        }
        TreePop();
    }
}

void AdjacencyList::SetJson(json &&j) const {
    IdPairs id_pairs = json::parse(std::string(std::move(j)));
    Erase();
    for (IdPair id_pair : id_pairs) Add(std::move(id_pair));
}

// Using a string representation so we can flatten the JSON without worrying about non-object collection values.
json AdjacencyList::ToJson() const { return json(Get()).dump(); }
