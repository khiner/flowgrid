#include "Vector.h"

#include "Core/Store/Store.h"
#include "Helper/Hex.h"

#include "imgui_internal.h"

template<HasId ChildType> void Vector<ChildType>::RefreshFromChangedPathPairs(const std::unordered_set<StorePath, PathHash> &changed_path_pairs) {
    for (const StorePath &path_pair : changed_path_pairs) {
        const auto child_it = FindByPathPair(path_pair);
        if (child_it != Value.end()) {
            Value.erase(child_it);
        } else {
            const auto &[path_prefix, path_segment] = GetPathPrefixAndSegment(path_pair);
            auto new_child = Creator(this, path_prefix, path_segment);
            u32 index = Ids.IndexOf(new_child->Id);
            Value.insert(Value.begin() + index, std::move(new_child));
        }
    }
}

template<HasId ChildType> void Vector<ChildType>::Refresh() {
    if (!ChangedPaths.contains(Id)) return;

    // xxx this is redundant but need to ensure it's run first.
    //  Can we change `Field::RefreshChanged` to guarantee that children will be refreshed before parents?
    Ids.Refresh();

    // Find all unique prefix-ids in the changed paths.
    const auto &changed_paths = ChangedPaths.at(Id).second;
    std::unordered_set<StorePath, PathHash> changed_path_prefixes;
    for (const auto &path : changed_paths) {
        // Path is already relative to this vector's path.
        auto it = path.begin();
        changed_path_prefixes.insert(*it / *std::next(it));
    }

    RefreshFromChangedPathPairs(changed_path_prefixes);
}

template<HasId ChildType> void Vector<ChildType>::RefreshFromJson(const json &j) {
    auto &&flattened = std::move(j).flatten();

    // Get all changed path prefixes.
    std::unordered_set<StorePath, PathHash> json_path_pairs;
    for (auto &&[key, value] : flattened.items()) {
        const StorePath path = key; // Already relative.
        auto it = path.begin();
        it++; // First segment is just a "/".
        if (it->string() == Ids.PathSegment) {
            Ids.SetJson(std::move(value));
            Ids.Refresh();
        } else {
            json_path_pairs.insert(*it / *std::next(it));
        }
    }

    std::unordered_set<StorePath, PathHash> changed_path_pairs = json_path_pairs;
    for (const auto &child : Value) {
        const auto path = child->Path.lexically_relative(Path);
        auto it = path.begin();
        const StorePath path_pair = *it / *std::next(it);
        if (changed_path_pairs.contains(path_pair)) changed_path_pairs.erase(path_pair);
        else changed_path_pairs.insert(path_pair);
    }

    RefreshFromChangedPathPairs(changed_path_pairs);
}

using namespace ImGui;

template<HasId ChildType> void Vector<ChildType>::RenderValueTree(bool annotate, bool auto_select) const {
    if (Value.empty()) {
        TextUnformatted(std::format("{} (empty)", Name).c_str());
        return;
    }

    // todo move the three repetitions of this block into `TreeNode`? (The other one is in `Component::RenderValueTree`.)
    if (auto_select) {
        const bool is_changed = IsChanged();
        SetNextItemOpen(is_changed);
        // Scroll to the current tree node row:
        if (is_changed && IsItemVisible()) ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
    }

    if (TreeNode(Name)) {
        for (u32 i = 0; i < Value.size(); i++) {
            if (auto_select) {
                const bool is_changed = Value.at(i)->IsChanged();
                SetNextItemOpen(is_changed);
                // Scroll to the current tree node row:
                if (is_changed && IsItemVisible()) ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
            }

            if (TreeNode(to_string(i))) {
                Value.at(i)->RenderValueTree(annotate, auto_select);
                TreePop();
            }
        }
        TreePop();
    }
}

// Explicit instantiations.
#include "Project/Audio/Graph/AudioGraphNode.h"

template struct Vector<AudioGraphNode>;
