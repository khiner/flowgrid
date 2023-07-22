#pragma once

#include <concepts>

#include "Core/Field/Field.h"

#include "imgui_internal.h"

template<typename T>
concept HasId = requires(T t) {
    { t.Id } -> std::same_as<const ID &>;
};

// A component whose children are created/destroyed dynamically, with vector-ish semantics.
// Like a `Field`, this wraps around an inner `Value` instance, which in this case is a `std::vector` of `std::unique_ptr<ChildType>`.
// Components typically own their children directly, declaring them as concrete instances on the stack via the `Prop` macro.
// Using `Vector` allows for runtime creation/destruction of children, and for child component types without the header having access to the full child definition.
// (It needs access to the definition for `ChildType`'s default destructor, though, since it uses `std::unique_ptr`.)
// Thoughts:
// Tricky to delete and reconstruct objects based solely on the store.
// But that's exactly what we absolutely need to do.
// If this `Creators` pattern doesn't work (e.g. string_view may not be valid anymore?),
// we may need to _only_ support components that can be constructed with `ComponentArgs` values alone: (Parent/PathSegment/MetaStr),
// using strings instead of string_views.
template<HasId ChildType> struct Vector : Field {
    using Field::Field;

    // Type must be constructable from `ComponentArgs`.
    template<typename ChildSubType, typename... Args>
        requires std::derived_from<ChildSubType, ChildType>
    void EmplaceBack(string_view path_segment = "", string_view meta_str = "", Args &&...other_args) {
        Value.emplace_back(std::make_unique<ChildSubType>(ComponentArgs{this, path_segment, meta_str, std::to_string(Value.size())}, std::forward<Args>(other_args)...));
        Creators[Value.back()->Id] = [=]() {
            return std::make_unique<ChildSubType>(ComponentArgs{this, path_segment, meta_str}, other_args...);
        };
        // Value.emplace_back(Creators.back()());
        // Ids.PushBack_(Value.back()->Id);
    }

    // Idea for more general args.
    // template<typename ChildSubType, typename... Args>
    //     requires std::derived_from<ChildSubType, ChildType>
    // void EmplaceBack(Args &&...args_without_parent) {
    //     Value.emplace_back(std::make_unique<ChildSubType>(this, std::forward<Args>(args_without_parent)...));
    //     // Store a lambda that captures the arguments and can create a new object.

    // }

    struct Iterator : std::vector<std::unique_ptr<ChildType>>::const_iterator {
        using Base = std::vector<std::unique_ptr<ChildType>>::const_iterator;

        Iterator(Base it) : Base(it) {}
        const ChildType *operator*() const { return Base::operator*().get(); }
        ChildType *operator*() { return Base::operator*().get(); }
    };

    Iterator begin() const { return Value.cbegin(); }
    Iterator end() const { return Value.cend(); }

    ChildType *back() const { return Value.back().get(); }
    ChildType *operator[](u32 i) const { return Value[i].get(); }

    u32 Size() const { return Value.size(); }

    void Refresh() override;

    void EraseAt(ID id) const {
        auto it = std::find_if(Value.begin(), Value.end(), [id](const auto &child) { return child->Id == id; });
        if (it != Value.end()) {
            it->get()->Erase();
        }
    }

    void RenderValueTree(bool annotate, bool auto_select) const override {
        if (Value.empty()) {
            ImGui::TextUnformatted(std::format("{} (empty)", Name).c_str());
            return;
        }

        // todo move the three uses of this block into `TreeNode`? (The other one is in `Component::RenderValueTree`.)
        if (auto_select) {
            const bool is_changed = ChangedComponentIds.contains(Id);
            ImGui::SetNextItemOpen(is_changed);
            // Scroll to the current tree node row:
            if (is_changed && ImGui::IsItemVisible()) ImGui::ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
        }

        if (TreeNode(Name)) {
            for (u32 i = 0; i < Value.size(); i++) {
                if (auto_select) {
                    const bool is_changed = ChangedComponentIds.contains(Value.at(i)->Id);
                    ImGui::SetNextItemOpen(is_changed);
                    // Scroll to the current tree node row:
                    if (is_changed && ImGui::IsItemVisible()) ImGui::ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
                }

                if (TreeNode(to_string(i))) {
                    Value.at(i)->RenderValueTree(annotate, auto_select);
                    ImGui::TreePop();
                }
            }
            ImGui::TreePop();
        }
    }

private:
    std::vector<std::unique_ptr<ChildType>> Value;
    std::map<ID, std::function<std::unique_ptr<ChildType>()>> Creators; // Stores the object creators
};
