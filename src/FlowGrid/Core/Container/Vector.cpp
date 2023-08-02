#include "Vector.h"

#include "Core/Store/Store.h"

#include "imgui_internal.h"
#include <range/v3/range/conversion.hpp>

template<HasId ChildType> void Vector<ChildType>::Refresh() {
    for (const StorePath prefix : ChildPrefixes) {
        const auto child_it = FindIt(prefix);
        if (child_it == Value.end()) {
            const auto &[path_prefix, path_segment] = GetPathPrefixAndSegment(prefix);
            auto new_child = Creator(this, path_prefix, path_segment);
            u32 index = ChildPrefixes.IndexOf(GetChildPrefix(new_child.get()));
            Value.insert(Value.begin() + index, std::move(new_child));
        }
    }
    std::erase_if(Value, [this](const auto &child) { return !ChildPrefixes.Contains(GetChildPrefix(child.get())); });
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
