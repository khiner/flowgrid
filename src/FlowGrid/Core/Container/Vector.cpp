#include "Vector.h"

#include "Core/Store/Store.h"
#include "Helper/Hex.h"

#include "imgui_internal.h"

template<HasId ChildType> void Vector<ChildType>::Refresh() {
    if (!ChangedPaths.contains(Id)) return;

    // Find all unique prefix-ids in the changed paths.
    const auto &changed_paths = ChangedPaths.at(Id).second;
    std::unordered_set<u32> changed_prefix_ids;
    for (const auto &path : changed_paths) {
        // Path is already relative to this vector's path.
        const std::string prefix_id_hex = path.begin()->string();
        const u32 prefix_id = HexToU32(prefix_id_hex);
        if (!CreatorByPrefixId.contains(prefix_id)) {
            throw std::runtime_error(std::format("Unknown prefix-id segment: {}", prefix_id));
        }
        changed_prefix_ids.insert(prefix_id);
    }

    for (const auto &prefix_id : changed_prefix_ids) {
        const std::string prefix_id_hex = U32ToHex(prefix_id);
        const auto vector_path = Path;
        const auto child_it = std::find_if(Value.begin(), Value.end(), [&prefix_id_hex, &vector_path](const auto &child) {
            const auto &child_path = child->Path;
            const auto relative_path = child_path.lexically_relative(vector_path);
            return relative_path.begin()->string() == prefix_id_hex;
        });

        if (child_it != Value.end()) {
            Value.erase(child_it);
        } else {
            // todo: insert back into the same position.
            //   This requires storing an auxiliary `Prop(PrimitiveVector<u32>, PrefixIds` tracking prefix-id order.
            Value.emplace_back(CreatorByPrefixId.at(prefix_id)());
        }
    }
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
